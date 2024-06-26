
<?php if ($this->ssl_enabled && $this->ssl_key) : ?>

<?php if ($this->redirection): ?>
<?php foreach ($this->aliases as $alias_url): ?>
server {
  listen       <?php print "*:{$http_ssl_port}"; ?>;
  listen       <?php print "[::]:{$http_ssl_port}"; ?>;
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

  ssl_certificate            <?php print $ssl_cert; ?>;
  ssl_certificate_key        <?php print $ssl_cert_key; ?>;
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

  <?php if ($ssl_chain_cert) { ?>
  # Required for OSCP stapling
  resolver 8.8.8.8 valid=500s;

  # Enable ocsp stapling
  # (mechanism by which a site can convey certificate revocation information to visitors in a privacy-preserving, scalable manner)
  ssl_stapling on;
  ssl_trusted_certificate <?php print $ssl_chain_cert; ?>;
  <?php } ?>

  keepalive_timeout          70;
  rewrite ^ $scheme://<?php print $this->redirection; ?>$request_uri? permanent;
}
<?php endforeach; ?>
<?php endif; ?>

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
    $db_type = 'mysql';
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
  listen        <?php print "*:{$http_ssl_port}"; ?>;
  listen        <?php print "[::]:{$http_ssl_port}"; ?>;
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
  ssl_certificate            <?php print $ssl_cert; ?>;
  ssl_certificate_key        <?php print $ssl_cert_key; ?>;

  ssl_protocols              TLSv1 TLSv1.1 TLSv1.2;
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

  <?php if ($ssl_chain_cert) { ?>
  # Required for OSCP stapling
  resolver 8.8.8.8 valid=500s;

  # Enable ocsp stapling
  # (mechanism by which a site can convey certificate revocation information to visitors in a privacy-preserving, scalable manner)
  ssl_stapling on;
  ssl_trusted_certificate <?php print $ssl_chain_cert; ?>;
  <?php } ?>

  keepalive_timeout          70;
  <?php print $extra_config; ?>
  include                    <?php print $server->include_path; ?>/nginx_vhost_common.conf;
}

<?php endif; ?>

<?php
  // Generate the standard virtual host too.
  // include(provision_class_directory('Provision_Config_Nginx_Site') . '/vhost.tpl.php');
  include('/var/aegir/.drush/provision_symbiotic/tpl/custom-nginx-vhost.tpl.php');
?>
