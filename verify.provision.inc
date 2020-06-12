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

  // Allow our monitoring to fetch the php-fpm status page
  if (d()->name == '@hm' || d()->name == '@hostmaster') {
    $phpfpm_socket_path = Provision_Service_http_nginx::getPhpFpmSocketPath();

    // Wee hack to allow requests from our public IPs, since the
    // wget runs against the official/public hostname.
    $allow = ['127.0.0.1', '::1'];
    $hosts = file_get_contents('/etc/hosts');

    $lines = explode("\n", $hosts);

    foreach ($lines as $line) {
      if (preg_match('/^([^\s]+)\s+' . d()->uri . '/', $line, $matches)) {
        $allow[] = $matches[1];
      }
    }

    $htaccess .= "  location ~ ^/fpm-(status|ping)\$ {\n";
    $htaccess .= "    tcp_nopush off;\n";
    $htaccess .= "    access_log off;\n";
    $htaccess .= "    keepalive_requests 0;\n";

    foreach ($allow as $a) {
      $htaccess .= "    allow $a;\n";
    }

    $htaccess .= "    deny all;\n";
    $htaccess .= "    fastcgi_pass unix:$phpfpm_socket_path;\n";
    $htaccess .= "  }\n\n";

    // IP whitelisting, if enabled
    $whitelist = variable_get('provision_symbiotic_whitelist');

    if ($whitelist) {
      $whitelist = explode(',', $whitelist);

      $htaccess .= "  # IP whitelisting by provision_symbiotic\n";

      foreach ($whitelist as $ip) {
        $ip = trim($ip);
        $htaccess .= "  allow $ip;\n";
      }

      $htaccess .= "  deny all;\n";
    }
  }

  // D8 extern support
  if (drush_drupal_major_version() == 8) {
    // Depends on Aegir 7.x-3.152: https://www.drupal.org/project/provision/issues/2791095#comment-12717109
    // See also: https://www.drupal.org/project/provision/issues/3000599
    $phpfpm_socket_path = Provision_Service_http_nginx::getPhpFpmSocketPath();

    $htaccess .= 'location ~ ^/vendor/civicrm/civicrm-core/extern/.*\.php {
      tcp_nopush off;
      keepalive_requests 0;
      try_files $uri =404;
      fastcgi_pass unix:' . $phpfpm_socket_path . ';
    }' . "\n\n";

    $htaccess .= 'location ~ ^/vendor/civicrm/civicrm-core/packages/kcfinder/.*\.php {
      tcp_nopush off;
      keepalive_requests 0;
      try_files $uri =404;
      fastcgi_pass unix:' . $phpfpm_socket_path . ';
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
    if (!provision_symbiotic_civicrm_init()) {
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

    $extensions_suffix = NULL;

    if (preg_match('/civi-go.net$/', $host)) {
      // iXiam default
      $extensions_suffix = 'files/civicrm/ext';
    }
    elseif (file_exists("$root/sites/$host/modules/extensions/")) {
      // Symbiotic default
      $extensions_suffix = 'modules/extensions';
    }
    elseif (file_exists("$root/sites/$host/modules/civi-extensions/")) {
      // Legacy Symbiotic
      $extensions_suffix = 'modules/civi-extensions';
    }
    elseif (file_exists("$root/sites/$host/civiext/")) {
      // civicrm.org
      $extensions_suffix = 'civiext';
    }
    elseif (file_exists("$root/sites/$host/files/civicrm/extensions/")) {
      $extensions_suffix = 'files/civicrm/extensions';
    }
    elseif (file_exists("$root/sites/$host/files/civicrm/ext/")) {
      // CiviCRM default
      $extensions_suffix = 'files/civicrm/ext';
    }

    // Symbiotic default
    if (!$extensions_suffix) {
      $extensions_suffix = 'modules/extensions';
    }

    Civi::settings()->set('extensionsDir', "$root/sites/$host/$extensions_suffix/");
    Civi::settings()->set('extensionsURL', "https://$host/sites/$host/$extensions_suffix");

    Civi::settings()->set('imageUploadDir', "$root/sites/$host/files/civicrm/persist/contribute");
    Civi::settings()->set('imageUploadURL', "https://$host/sites/$host/files/civicrm/persist/contribute");

    _provision_symbiotic_disable_jobs_on_devsites();

    // We can't auto-configure sites without a token.
    // This includes the model site, the hostmaster, or sites not managed through hosting_restapi.
    if (!d()->hosting_restapi_token) {
      return;
    }

    // Migrate to CiviCRM settings instead of Drupal variables
    if (drush_drupal_major_version() == 7) {
      $migrate = [
        'hosting_restapi_initial_done',
        'hosting_restapi_token',
        'hosting_restapi_hostmaster',
      ];

      foreach ($migrate as $setting) {
        $val = Civi::settings()->get($setting);

        if (!$val) {
          $val = variable_get($setting);
          Civi::settings()->set($setting, $val);
        }
      }
    }

    if (!Civi::settings()->get('hosting_restapi_token')) {
      Civi::settings()->set('hosting_restapi_token', d()->hosting_restapi_token);
      Civi::settings()->set('hosting_restapi_hostmaster', d()->hosting_restapi_hostmaster);
    }

    // Change the CiviCRM site_id, if necessary (ex: cloning from a template site)
    $site_id = CRM_Utils_System::getSiteID();

    if ($site_id == 'template' && strpos('template', $host) === FALSE && strpos('modele', $host) === FALSE) {
      $sid = md5('sid_' . time() . $host);
      civicrm_api3('Setting', 'create', ['domain_id' => 'all', 'site_id' => $sid]);
    }

    // The "verify" task can run regularly. Check to make sure we only run once,
    // after the initial clone.
    if (Civi::settings()->get('hosting_restapi_initial_done')) {
      return;
    }

    // @todo Drupal8 is not currently supported
    if (drush_drupal_major_version() == 8) {
      return;
    }

    // Request the site configuration
    $result = drupal_http_request(d()->hosting_restapi_hostmaster . '/hosting/api/site/config?url=' . d()->uri . '&token=' . d()->hosting_restapi_token);
    $config = json_decode($result->data, TRUE);

    variable_set('site_name', $config['data']['site']['name']);

    # [ML] provision_spark sets no-reply-foo and this makes sense for symbiotic too,
    # because we don't have an SMTP provider setup at this point.
    # variable_set('site_mail', $config['data']['organization']['email']);

    try {
      civicrm_api3('Contact', 'Create', [
        'id' => 1,
        'organization_name' => $config['data']['organization']['name'], // FIXME: should be the name provided in custom field
        'display_name' => $config['data']['organization']['name'],
        // 'phone' => $config['organization']['phone'],
        // 'email' => $config['data']['organization']['email'],
      ]);

      // Update uid=2, which is the default admin account (uid=1 is the superadmin, we do not give access to that)
      db_query('UPDATE users SET mail = :mail, init = :mail, created = unix_timestamp(), access = 0, login = 0 WHERE uid = 2', [
        ':mail' => $config['data']['individual']['email'],
      ]);

      // Update cid=3, which is the default individual contact
      civicrm_api3('Contact', 'Create', [
        'id' => 3,
        'first_name' => $config['data']['individual']['first_name'],
        'last_name' => $config['data']['individual']['last_name'],
      ]);

      civicrm_api3('Email', 'Create', [
        'id' => 3,
        'email' => $config['data']['individual']['email'],
      ]);
    }
    catch (Exception $e) {
      drush_log('Symbiotic: failed to set some CiviCRM configurations: ' . $e->getMessage(), 'warning');
    }

    variable_set('hosting_restapi_initial_done', 1);
  }
}

/**
 * Automatically sets Environment=Development and disables iATS logins on dev sites.
 *
 * NB: we disable by changing the login, since the iATS recurring Scheduled Job
 * will still run even if the payment processor is disabled. Since recent 5.x CiviCRM
 * releases, jobs are not run when Environment=Development, but we are still doing
 * this just in case someone accidentally changes the environment back to 'production'.
 */
function _provision_symbiotic_disable_jobs_on_devsites() {
  // Match against dev patterns:
  // - crm-dev.* (SymbioTIC)
  // - *.symbiodev.xyz (SymbioTIC)
  // - *.crm-dev.* (Jvillage)
  // - *.www-test.* (CiviCRM.org)
  $is_dev = FALSE;

  if (preg_match('/^crm-dev\./', d()->uri)) {
    $is_dev = TRUE;
  }
  if (preg_match('/symbiodev.xyz/', d()->uri)) {
    $is_dev = TRUE;
  }
  if (preg_match('/\.crm-dev\./', d()->uri)) {
    $is_dev = TRUE;
  }
  if (preg_match('/\.www-test\./', d()->uri)) {
    $is_dev = TRUE;
  }

  if (!$is_dev) {
    return;
  }

  drush_log("Symbiotic: This is a dev site, setting environment=Development and checking if iATS needs to be disabled", 'ok');

  Civi::settings()->set('environment', 'Development');

  $result = civicrm_api3('PaymentProcessor', 'get', [
    'option.limit' => 0,
  ]);

  foreach ($result['values'] as $key => $val) {
    // Don't rename the login if it's the test credentials
    if ($val['user_name'] == 'TEST88') {
      continue;
    }

    if (strpos($val['user_name'], 'DISABLEDBYAEGIR') !== FALSE) {
      drush_log("Symbiotic: processor_id={$val['id']} credentials already renamed ({$val['user_name']})", 'ok');
      continue;
    }

    if (preg_match('/iats/i', $val['class_name']) || preg_match('/eway/i', $val['class_name'])) {
      $new_user_name = $val['user_name'] . 'DISABLEDBYAEGIR';

      civicrm_api3('PaymentProcessor', 'create', [
        'id' => $val['id'],
        'user_name' => $new_user_name,
      ]);

      drush_log("Symbiotic: {$val['class_name']} processor_id={$val['id']} credentials have been disabled (renamed login to: {$new_user_name})", 'ok');
    }
  }

  // Disable the Recurring Contribution iATS job
  $job_names = [
    'iATS Payments Recurring Contributions',
    'iATS Payments Verification',
  ];

  foreach ($job_names as $job_name) {
    $result = civicrm_api3('Job', 'get', [
      'name' => $job_name,
      'option.limit' => 0,
    ]);

    foreach ($result['values'] as $key => $val) {
      if ($val['is_active']) {
        civicrm_api3('Job', 'create', [
          'id' => $key,
          'is_active' => 0,
        ]);

        drush_log("Symbiotic: Disabled iATS Scheduled Job: {$val['name']}", 'ok');
      }
    }
  }
}
