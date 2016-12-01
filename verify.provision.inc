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
  // This is redundant with hosting_https, but we are not using it
  // everywhere yet.
  if (!module_exists('hosting_https')) {
    $htaccess =
        "  location ^~ /.well-known/acme-challenge {\n"
      . "    alias /var/aegir/config/letsencrypt.d/well-known/acme-challenge;\n"
      . "    try_files \$uri 404;\n"
      . "  }\n\n";
  }

  // Git repos can be cloned through https
  $htaccess .= 'location ~ /\.git { return 404; }' . "\n\n";

  return $htaccess;
}

/**
 * Implements hook_post_provision_verify().
 */
function drush_provision_symbiotic_post_provision_verify($url = NULL) {
  if (d()->type == 'site') {
    // Only run on CiviCRM sites.
    // For example, the "hostmaster" site does not run CiviCRM.
    if (! function_exists('civicrm_initialize')) {
      return;
    }

    // Ignore CiviCRM 4.6 and previous.
    if (! method_exists('Civi', 'settings')) {
      return;
    }

    civicrm_initialize();

    // Make sure that CiviCRM path/url settings are correct.
    $root = d()->root;
    $host = d()->uri;

    Civi::settings()->set('userFrameworkResourceURL', "https://$host/sites/all/modules/civicrm");
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
