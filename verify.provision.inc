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
  $htaccess = 'location ~ /\.git { return 404; }' . "\n\n";

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
    drush_log('provision_symbiotic: drush_provision_symbiotic_post_provision_verify start', 'ok');
    $output = _provision_civicrm_run_cv_command('php:script ' . __DIR__ . '/cv/post-verify.php ' . implode(' ', array_map('escapeshellarg', [
      d()->root, d()->uri, d()->site_path, d()->hosting_restapi_token, d()->hosting_restapi_hostmaster,
    ])));
    foreach ($output as $line) {
      drush_log('provision_symbiotic: ' . $line, strpos($line, 'warning') === FALSE ? 'ok' : 'warning');
    }
    drush_log('provision_symbiotic: drush_provision_symbiotic_post_provision_verify end', 'ok');
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
