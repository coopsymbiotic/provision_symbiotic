<?php

/**
 * Implements hook_provision_apache_vhost_config().
 *
 * Inject the relevant Apache configuration in the site vhost
 */
function provision_symbiotic_provision_apache_vhost_config($uri, $data = null) {
  // Block access to the htaccess-custom file
  // It's a form of configuration file that is not meant to be public.
  $htaccess  =
      "<Location \"/xmlrpc.php\">\n"
    . "  Order deny,allow\n"
    . "  Deny from all\n"
    . "</Location>\n";

  // Workaround for Moisaico bug
  // https://github.com/veda-consulting/uk.co.vedaconsulting.mosaico/issues/160
  $htaccess .= 'RewriteRule "^/templates/(.*)$" /vendor/civicrm/uk.co.vedaconsulting.mosaico/packages/mosaico/templates/$1 [R=301,L]' . "\n\n";

  // Git repos can be cloned through https
  $htaccess .= 'RedirectMatch 404 ^/\.git.*$' . "\n\n";

  return $htaccess;
}

/**
 * Implements hook_provision_nginx_vhost_config().
 *
 * Inject Nginx configuration in the site vhost.
 */
function provision_symbiotic_provision_nginx_vhost_config($uri, $data = null) {
  // Git repos can be cloned through https
  $htaccess .= 'location ~ /\.git { return 404; }' . "\n\n";

  // Workaround for Moisaico bug
  // https://github.com/veda-consulting/uk.co.vedaconsulting.mosaico/issues/160
  $htaccess .= 'rewrite ^/templates/(.*)$ /vendor/civicrm/uk.co.vedaconsulting.mosaico/packages/mosaico/templates/$1 permanent;' . "\n\n";

  // Required for REST API access (and must be outside the location block)
  $htaccess .= 'fastcgi_param CIVICRM_SETTINGS \'' . d()->root . '/sites/' . d()->uri . '/civicrm.settings.php\';' . "\n\n";

  // D8 extern support
  if (drush_drupal_major_version() == 8) {
    $htaccess .= 'location ~ ^/vendor/civicrm/civicrm-core/extern/.*\.php {
      tcp_nopush off;
      keepalive_requests 0;
      try_files $uri =404;
      fastcgi_pass unix:/var/run/php/php7.0-fpm.sock;
    }' . "\n\n";
  }

  return $htaccess;
}

/**
 * Implements hook_post_provision_verify().
 */
function drush_provision_symbiotic_post_provision_verify($url = NULL) {
  if (d()->type == 'site') {
    // Only run on CiviCRM sites.
    // For example, the "hostmaster" site does not run CiviCRM.
    if (drush_drupal_major_version() == 8 && Drupal::hasService('civicrm')) {
      \Drupal::service('civicrm')->initialize();
    }
    elseif (function_exists('civicrm_initialize')) {
      civicrm_initialize();
    }
    else {
      return;
    }

    // Ignore CiviCRM 4.6 and previous.
    if (! method_exists('Civi', 'settings')) {
      return;
    }

    // Make sure that CiviCRM path/url settings are correct.
    $root = d()->root;
    $host = d()->uri;

    if (drush_drupal_major_version() == 8) {
      Civi::settings()->set('userFrameworkResourceURL', "https://$host/vendor/civicrm/civicrm-core");
    }
    else {
      // CiviCRM might be in sites/all/modules or in the profile directory.
      $module = _provision_civicrm_get_package_info();
      $url = $module['filename'];
      $url = substr($url, strlen(d()->root));
      $url = substr($url, 0, - strlen('drupal/civicrm.module'));
      $url = "https://$host$url";

      Civi::settings()->set('userFrameworkResourceURL', $url);
    }

    Civi::settings()->set('uploadDir', "$root/sites/$host/files/civicrm/upload/");
    Civi::settings()->set('customFileUploadDir', "$root/sites/$host/files/civicrm/custom/");

    Civi::settings()->set('customPHPPathDir', NULL);
    Civi::settings()->set('customTemplateDir', NULL);

    $extensions_suffix = 'modules/extensions';

    if (file_exists("$root/sites/$host/modules/civi-extensions/")) {
      $extensions_suffix = 'modules/civi-extensions';
    }

    if (file_exists("$root/sites/$host/civiext/")) {
      $extensions_suffix = 'civiext';
    }

    Civi::settings()->set('extensionsDir', "$root/sites/$host/$extensions_suffix/");
    Civi::settings()->set('extensionsURL', "https://$host/sites/$host/$extensions_suffix");

    Civi::settings()->set('imageUploadDir', "$root/sites/$host/files/civicrm/persist/contribute");
    Civi::settings()->set('imageUploadURL', "https://$host/sites/$host/files/civicrm/persist/contribute");

    // The "verify" task can run regularly. Check to make sure we only run once,
    // after the initial clone.
    if (drush_drupal_major_version() == 8) {
      // Drupal8 is not supported for now. We don't really use this anyway.
      return;
    }

    if (variable_get('hosting_restapi_initial_done', NULL)) {
      return;
    }

    // We can't auto-configure sites without a token.
    // This includes the model site, the hostmaster, or sites not managed through hosting_restapi.
    if (! d()->hosting_restapi_token) {
      return;
    }

    variable_set('hosting_restapi_token', d()->hosting_restapi_token);
    variable_set('hosting_restapi_hostmaster', d()->hosting_restapi_hostmaster);

    // Request the site configuration
    $result = drupal_http_request(d()->hosting_restapi_hostmaster . '/sites/config?url=' . d()->uri . '&token=' . d()->hosting_restapi_token);
    $config = json_decode($result->data);

    variable_set('site_name', $config['organization']['name']);
    variable_set('site_mail', $config['organization']['email']);

    try {
      civicrm_api3('Contact', 'Create', array(
        'id' => 1,
        'organization_name' => $config['organization']['name'],
        'display_name' => $config['organization']['name'],
        'phone' => $config['organization']['phone'],
        'email' => $config['organization']['email'],
      ));

/*
      civicrm_api3('Phone', 'Create', array(
        'contact_id' => 1,
        'phone' => $config['organization']['phone'],
      ));
*/

      // TODO: individual contact + create new user account & notify?
      // however.. we can't notify & include the password by email
      // and if the notify includes a one-time link, it would expire from the login-reset task (if it can work on uid=2).
    }
    catch (Exception $e) {
      drush_log('Symbiotic: failed to set some CiviCRM configurations: ' . $e->getMessage(), 'warning');
    }

    variable_set('hosting_restapi_initial_done', 1);
  }
}
