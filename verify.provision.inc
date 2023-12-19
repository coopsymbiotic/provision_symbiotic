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

  // Workaround for extensions with css/js (ex: shoreditch) in the /vendor directory
  // on multilingual sites with language prefix.
  // On nginx (which we recommend over Apache), we use provision_customhtaccess and
  // store this in the custom-nginx.conf of each site (but we should probably add here too).
  $htaccess .=
      '<IfModule mod_rewrite.c>' . "\n"
    . '  RewriteEngine on' . "\n"
    . '  RewriteRule ^/[a-z][a-z]/vendor/(.*)$ /vendor/$1 [L]' . "\n"
    . '</IfModule>' . "\n\n";

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
    $local_ips = ['127.0.0.1', '::1'];
    $hosts = file_get_contents('/etc/hosts');

    $lines = explode("\n", $hosts);

    foreach ($lines as $line) {
      if (preg_match('/^([^\s]+)\s+' . d()->uri . '/', $line, $matches)) {
        if (!in_array($matches[1], $local_ips)) {
          $local_ips[] = $matches[1];
        }
      }
    }

    // Add other IPs from Symbiotic infra, ex: for inventory access
    // The "system_whitelist" restricts access to fpm and ansible inventory
    // The other "whitelist" is optional, and restricts access to Aegir itself
    $provision_symbiotic_system_whitelist = $local_ips;

    if ($t = variable_get('provision_symbiotic_system_whitelist')) {
      $provision_symbiotic_system_whitelist = explode(',', $t);
      $provision_symbiotic_system_whitelist = array_merge($local_ips, $provision_symbiotic_system_whitelist);
    }

    $htaccess .= "  location ~ ^/fpm-(status|ping)\$ {\n";
    $htaccess .= "    tcp_nopush off;\n";
    $htaccess .= "    access_log off;\n";
    $htaccess .= "    keepalive_requests 0;\n";

    foreach ($provision_symbiotic_system_whitelist as $ip) {
      $htaccess .= "    allow $ip;\n";
    }

    $htaccess .= "    deny all;\n";
    $htaccess .= "    fastcgi_pass unix:$phpfpm_socket_path;\n";
    $htaccess .= "  }\n\n";

    // Restricts access to Aegir itself, if enabled
    $whitelist = variable_get('provision_symbiotic_whitelist');

    if ($whitelist) {
      $whitelist = explode(',', $whitelist);
      $whitelist = array_merge($local_ips, $whitelist);

      $htaccess .= "  # IP whitelisting by provision_symbiotic\n";

      foreach ($whitelist as $ip) {
        $ip = trim($ip);
        $htaccess .= "  allow $ip;\n";
      }

      $htaccess .= "  deny all;\n";
    }

    // Restrict access to the Ansible inventory
    $htaccess .= "  location /inventory {\n";
    $htaccess .= "    tcp_nopush off;\n";
    $htaccess .= "    access_log off;\n";
    $htaccess .= "    keepalive_requests 0;\n";

    foreach ($provision_symbiotic_system_whitelist as $ip) {
      $htaccess .= "    allow $ip;\n";
    }

    $htaccess .= "    deny all;\n";
    $htaccess .= "    try_files \$uri @drupal;\n";
    $htaccess .= "  }\n";
  }

  // D8 extern support
  if (drush_drupal_major_version() >= 8) {
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
  if (d()->type === 'site') {
    // Only run on CiviCRM sites.
    // For example, the "hostmaster" site does not run CiviCRM.
    if (!provision_symbiotic_civicrm_init()) {
      return;
    }

    // Ignore CiviCRM 4.6 and previous.
    if (! method_exists('Civi', 'settings')) {
      return;
    }

    drush_log('provision_symbiotic: drush_provision_symbiotic_post_provision_verify start', 'ok');

    // Make sure that CiviCRM path/url settings are correct.
    $root = d()->root;
    $host = d()->uri;

    // Only update the Directory settings if not using relative paths
    $settings = [
      'uploadDir' => "$root/sites/$host/files/civicrm/upload",
      'customFileUploadDir' => "$root/sites/$host/files/civicrm/custom",
      'imageUploadDir' => "$root/sites/$host/files/civicrm/persist/contribute",
      'imageUploadURL' => "https://$host/sites/$host/files/civicrm/persist/contribute",
    ];

    foreach ($settings as $setting => $default) {
      $oldval = Civi::settings()->get($setting);

      if (strpos($oldval, '[civicrm.files]') === FALSE) {
        Civi::settings()->set($setting, $default);
      }
    }

    Civi::settings()->set('customPHPPathDir', NULL);
    Civi::settings()->set('customTemplateDir', NULL);

    $extensions_suffix = NULL;
    $extensionDir = Civi::settings()->get('extensionsDir');

    drush_log('provision_symbiotic: extensionsDir = ' . $extensionDir, 'ok');

    // Only update the extensionsDir if not using relative paths
    // @todo This was moved to hosting_civicrm and could be removed here
    // once we update hosting_civicrm everywhere
    if (strpos($extensionDir, '[civicrm.files]') === FALSE) {
      if (preg_match('/civi-go.net$/', $host)) {
        // iXiam default
        $extensions_suffix = 'files/civicrm/ext';
      }
      elseif (file_exists("$root/sites/$host/modules/extensions/")) {
        // Symbiotic default
        $extensions_suffix = 'modules/extensions';
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
    }

    _provision_symbiotic_disable_jobs_on_devsites();

    // Change the CiviCRM site_id, if necessary (ex: cloning from a template site)
    $site_id = CRM_Utils_System::getSiteID();

    if ($site_id == 'template' && strpos($host, 'template') === FALSE && strpos($host, 'modele') === FALSE) {
      $sid = md5('sid_' . time() . $host);
      civicrm_api3('Setting', 'create', ['domain_id' => 'all', 'site_id' => $sid]);
    }

    // Flush cache - copy-pasted from hosting_civicrm - @todo remove when we remove the above settings updates (now in hosting_civicrm)
    // Run as a shell command, instead of PHP, so that fatal errors do not crash the verify task
    $command = 'cd ' . escapeshellarg(d()->site_path) . ' && drush cc drush && drush @' . escapeshellarg(d()->uri) . ' cvapi --out=json system.flush';
    drush_log(dt("CiviCRM: calling cache clear: %command", ['%command' => $command]), 'ok');
    $json = system($command, $retval);
    $output = json_decode($json, TRUE);
    if ($retval || !empty($output['is_error'])) {
      drush_log(dt("CiviCRM: cache clear failed (the ConfigAndLog should have more details): %output", ['%output' => $json]), 'warning');
    }
    elseif (!preg_match('/is_error.:0/', $json)) {
      drush_log(dt("CiviCRM: cache clear may have failed: %output", ['%output' => $json]), 'warning');
    }
    else {
      drush_log(dt("CiviCRM: cache clear success: %output", ['%output' => $json]), 'ok');
    }

    // We can't auto-configure sites without a token.
    // This includes the model site, the hostmaster, or sites not managed through hosting_restapi.
    if (!d()->hosting_restapi_token) {
      return;
    }

    if (!Civi::settings()->get('hosting_restapi_token')) {
      Civi::settings()->set('hosting_restapi_token', d()->hosting_restapi_token);
      Civi::settings()->set('hosting_restapi_hostmaster', d()->hosting_restapi_hostmaster);
    }

    // The "verify" task can run regularly. Check to make sure we only run once,
    // after the initial clone.
    if (Civi::settings()->get('hosting_restapi_initial_done')) {
      return;
    }

    // @todo Drupal8 is not currently supported
    if (drush_drupal_major_version() >= 8) {
      return;
    }

    // Request the site configuration
    $token = Civi::settings()->get('hosting_restapi_token');
    $hostmaster =  Civi::settings()->get('hosting_restapi_hostmaster');

    $result = drupal_http_request($hostmaster . '/hosting/api/site/config?url=' . d()->uri . '&token=' . $token);
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

    // Set the locale, if possible (depends on provision_spark)
    $locale = mb_strtolower($config['data']['site']['locale']);
    $locale = preg_replace('/_/', '', $locale);

    if (function_exists('drush_provision_spark_' . $locale)) {
      $f = "drush_provision_spark_" . $locale;
      $f();
    }
    else {
      $locale = substr($locale, 0, 2);

      if (function_exists('drush_provision_spark_' . $locale)) {
        $f = "drush_provision_spark_" . $locale;
        $f();
      }
    }

    Civi::settings()->set('hosting_restapi_initial_done', 1);

    // Get a one-time link (Drupal7)
    $account = user_load(2);

    if (!empty($account)) {
      $onetime = user_pass_reset_url($account) . '/login';
      $onetime = preg_replace('/http:/', 'https:', $onetime);

      $result = drupal_http_request($hostmaster . '/hosting/api/site/welcome', [
        'method' => 'POST',
        'data' => [
          'url' => d()->uri,
          'token' => $token,
          'loginurl' => $onetime,
        ],
        'headers' => ['Content-Type' => 'application/x-www-form-urlencoded'],
      ]);

      drush_log('Symbiotic: sending welcome one-time link: ' . $onetime . ' via ' . $hostmaster . ' == ' . print_r($result, 1), 'ok');
    }
  }
  elseif (d()->type === 'server') {
    // Based on provision/platform/verify.provision.inc (pre provision verify)
    // We allow www-data to access backups so that they can download backups
    // using hosting_restapi and our CiviCRM aegirbackups extension.
    // While this could mean that a site could access backups from another site,
    // it's mitigated by 1) our infra runs only code from trusted clients (we do
    // not let them deploy custom code on a shared server), and 2) backups are
    // short-lived.
    if (d()->name == '@server_master') {
      provision_file()->create_dir(d()->backup_path, dt('Backup'), 0710);
    }
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
  $uri = d()->uri;
  if (!provision_symbiotic_is_dev_site($uri)) {
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
