
<?php if ($this->https_key && $this->https_cert_ok) : ?>

<?php
$satellite_mode = drush_get_option('satellite_mode');
if (!$satellite_mode && $server->satellite_mode) {
  $satellite_mode = $server->satellite_mode;
}

$nginx_has_http2 = drush_get_option('nginx_has_http2');
if (!$nginx_has_http2 && $server->nginx_has_http2) {
  $nginx_has_http2 = $server->nginx_has_http2;
}

if ($nginx_has_http2) {
  $ssl_args = "ssl http2";
}
else {
  $ssl_args = "ssl";
}

// Locate the https cert, if there is one
// Support for wildcard certs and local overrides for a single cert,
// and otherwise fallsback on letsencrypt.
// Note that sometimes letsencrypt fails to generate a cert (DNS fail, vhost snafu, etc)
// so if the cert is not available on disk, we do not enable https.
// This logic should probably be elsewhere and set https_cert_ok to FALSE if not found.
$https_key_name = '';
$https_cert_name = '';
$domain_parts = explode('.', $this->uri);

// Remove the first part, ex: foo.bar.example.org becomes bar.example.org
// because by definition, a wildcard covers *.example.org,
// but could also be *.service.example.org.
array_shift($domain_parts);

while (!empty($domain_parts)) {
  $wildcard_name_test = '_wildcard.' . implode('.', $domain_parts);

  drush_log(dt("Provision Symbiotic: Testing if :cert exists", [':cert' => $wildcard_name_test]), 'ok');

  if (file_exists("/var/aegir/config/letsencrypt.d/$wildcard_name_test/privkey.pem")) {
    drush_log(dt("Provision Symbiotic: Found :cert exists", [':cert' => $wildcard_name_test]), 'ok');
    $https_key_name = "/var/aegir/config/letsencrypt.d/$wildcard_name_test/privkey.pem";
    $https_cert_name = "/var/aegir/config/letsencrypt.d/$wildcard_name_test/fullchain.pem";
    break;
  }

  array_shift($domain_parts);
}

// Support for custom certs from another cert authority
if (file_exists("/var/aegir/config/letsencrypt.d/{$this->uri}.override/privkey.pem")) {
  drush_log(dt("Provision Symbiotic: Found cert override"), 'ok');
  $https_key_name = "/var/aegir/config/letsencrypt.d/{$this->uri}.override/privkey.pem";
  $https_cert_name = "/var/aegir/config/letsencrypt.d/{$this->url}.override/fullchain.pem";
}

if (!$https_key_name && file_exists("/var/aegir/config/letsencrypt.d/{$this->uri}/privkey.pem")) {
  drush_log(dt("Provision Symbiotic: Found letsencrypt certificate"), 'ok');
  $https_key_name = "/var/aegir/config/letsencrypt.d/{$this->uri}/privkey.pem";
  $https_cert_name = "/var/aegir/config/letsencrypt.d/{$this->uri}/fullchain.pem";
}

// Classic self-signed cert
if (!$https_key_name && file_exists("/var/aegir/config/self_signed.d/{$this->uri}/openssl.key")) {
  drush_log(dt("Provision Symbiotic: Found self-signed certificate"), 'ok');
  $https_key_name = "/var/aegir/config/self_signed.d/{$this->uri}/openssl.key";
  $https_cert_name = "/var/aegir/config/self_signed.d/{$this->uri}/openssl.crt";
}

?>

<?php if ($https_key_name && $this->redirection): ?>
<?php foreach ($this->aliases as $alias_url): ?>
server {
  listen       <?php print "*:{$https_port} {$ssl_args}"; ?>;
  listen       <?php print "[::]:{$https_port} {$ssl_args}"; ?>;
<?php
  // if we use redirections, we need to change the redirection
  // target to be the original site URL ($this->uri instead of
  // $alias_url)
  if ($this->redirection && $alias_url == $this->redirection) {
    $this->uri = str_replace('/', '.', $this->uri);
    print "  server_name  {$this->uri};\n";
  }
  else {
    $alias_url = str_replace('/', '.', $alias_url);
    print "  server_name  {$alias_url};\n";
  }
?>

  ssl_certificate_key        <?php print $https_key_name; ?>;
  ssl_certificate            <?php print $https_key_name; ?>;
  ssl_protocols              TLSv1.2 TLSv1.3;
  ssl_ciphers                ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-SHA384:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES256-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA:ECDHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA:DES-CBC3-SHA:!aNULL:!eNULL:!LOW:!DES:!MD5:!EXP:!PSK:!SRP:!DSS;
  ssl_ecdh_curve             secp384r1;
  ssl_prefer_server_ciphers  on;

  # Generated using:
  # openssl dhparam -check -5 4096 > /etc/nginx/params.4096
  # (can be re-generated regularly in a cron job)
  <?php if (file_exists('/etc/nginx/params.4096')) { ?>
  ssl_dhparam /etc/nginx/params.4096;
  <?php } ?>

  ssl_session_cache shared:SSL:10m;
  ssl_session_timeout 10m;
  keepalive_timeout          70;

<?php
  // Extra config is allowed here because it lets us support exotic requirements
  // with provision_customhtaccess.
  print $extra_config;
?>

  location / {
    return 301 $scheme://<?php print $this->redirection; ?>$request_uri;
  }
}
<?php endforeach; ?>
<?php endif; ?>

<?php if ($https_key_name): ?>
server {
  include       fastcgi_params;

  # Block https://httpoxy.org/ attacks.
  fastcgi_param HTTP_PROXY "";

  fastcgi_param MAIN_SITE_NAME <?php print $this->uri; ?>;
  set $main_site_name "<?php print $this->uri; ?>";
  fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
  fastcgi_param HTTPS on;
<?php
  // If any of those parameters is empty for any reason, like after an attempt
  // to import complete platform with sites without importing their databases,
  // it will break Nginx reload and even shutdown all sites on the system on
  // Nginx restart, so we need to use dummy placeholders to avoid affecting
  // other sites on the system if this site is broken.
  if (!$db_type || !$db_name || !$db_user || !$db_passwd || !$db_host) {
    $db_type = 'mysqli';
    $db_name = 'none';
    $db_user = 'none';
    $db_passwd = 'none';
    $db_host = 'localhost';
  }
?>
  fastcgi_param db_type   <?php print urlencode($db_type); ?>;
  fastcgi_param db_name   <?php print urlencode($db_name); ?>;
  fastcgi_param db_user   <?php print urlencode($db_user); ?>;
  fastcgi_param db_passwd <?php print urlencode($db_passwd); ?>;
  fastcgi_param db_host   <?php print urlencode($db_host); ?>;
<?php
  // Until the real source of this problem is fixed elsewhere, we have to
  // use this simple fallback to guarantee that empty db_port does not
  // break Nginx reload which results with downtime for the affected vhosts.
  if (!$db_port) {
    $db_port = $this->server->db_port ? $this->server->db_port : '3306';
  }
?>
  fastcgi_param db_port   <?php print urlencode($db_port); ?>;
  listen        <?php print "*:{$https_port} {$ssl_args}"; ?>;
  listen        <?php print "[::]:{$https_port} {$ssl_args}"; ?>;
  server_name   <?php
    // this is the main vhost, so we need to put the redirection
    // target as the hostname (if it exists) and not the original URL
    // ($this->uri)
    if ($this->redirection) {
      print str_replace('/', '.', $this->redirection);
    } else {
      print $this->uri;
    }
    if (!$this->redirection && is_array($this->aliases)) {
      foreach ($this->aliases as $alias_url) {
        if (trim($alias_url)) {
          print " " . str_replace('/', '.', $alias_url);
        }
      }
    } ?>;
  root          <?php print "{$this->root}"; ?>;

  ssl                        on;
  ssl_certificate_key        /var/aegir/config/letsencrypt.d/<?php print $https_key_name; ?>/privkey.pem;
  ssl_certificate            /var/aegir/config/letsencrypt.d/<?php print $https_key_name; ?>/fullchain.pem;
  ssl_protocols              TLSv1.2 TLSv1.3;
  ssl_ciphers                ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA384:DHE-RSA-AES256-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES256-SHA:ECDHE-RSA-AES256-SHA:DHE-RSA-AES256-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES128-SHA:DHE-RSA-AES128-SHA:!aNULL:!eNULL:!LOW:!3DES:!DES:!MD5:!EXP:!PSK:!SRP:!DSS;
  ssl_ecdh_curve             secp384r1;
  ssl_prefer_server_ciphers  on;

  # Generated using:
  # openssl dhparam -check -5 4096 > /etc/nginx/params.4096
  # (can be re-generated regularly in a cron job)
  <?php if (file_exists('/etc/nginx/params.4096')) { ?>
  ssl_dhparam /etc/nginx/params.4096;
  <?php } ?>

  ssl_session_cache shared:SSL:10m;
  ssl_session_timeout 10m;
  keepalive_timeout          70;


<?php

  if ($this->site_enabled) {
    if ($extra_config) {
      print "  " . $extra_config . "\n\n";
    }
    print "  include {$server->include_path}/nginx_vhost_common.conf;\n";
  }
  else {
    print "  add_header Content-Type text/html;\n";
    print "  return 200 '<div><ul>"
    . "<li>(en) This web site has been disabled. Please contact the server administrators for more information.</li>"
    . "<li>(es) Este sitio web ha sido deshabilitado. Por favor contacte con los administradores para obtener m&agrave;s informaci&ograve;n.</li>"
    . "<li>(fr) Ce site a &eacute;t&eacute; d&eacute;sactiv&eacute;. Veuillez contacter les personnes responsables de l&#39;administration du serveur pour plus d&#39;information.</li>"
    . "</ul></div>';\n";
  }

?>
}
<?php endif; ?>

<?php endif; ?>

<?php
  // Generate the standard virtual host too.
  // include(provision_class_directory('Provision_Config_Nginx_Site') . '/vhost.tpl.php');
  include('/var/aegir/.drush/provision_symbiotic/tpl/custom-nginx-vhost.tpl.php');
?>
