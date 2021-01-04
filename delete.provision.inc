<?php

/**
 * Implementation of drush_hook_provision_delete.
 */
function drush_provision_symbiotic_provision_delete() {
  if (d()->type == 'site') {
    if (!file_exists('/var/aegir/config/symbiotic-dns')) {
      return;
    }

    $json = file_get_contents('/var/aegir/config/symbiotic-dns');
    $config = json_decode($json, TRUE);

    if (!$config) {
      drush_log(dt("Symbiotic: failed to parse the JSON in the symbiotic-dns config file."), 'warning');
      return;
    }

    if (!in_array($config['provider'], ['gandi', 'cloudflare'])) {
      drush_log(dt("Symbiotic: unsupported DNS provider: %name", ['%name' => $config['provider']]), 'warning');
      return;
    }

    $host = d()->uri;
    $cut = strpos($host, $config['domain']);

    if ($cut === FALSE) {
      drush_log(dt("Symbiotic: DNS: Ignoring host %host because it does not match %domain", ['%host' => $host, '%domain' => $config['domain']]), 'ok');
      return;
    }

    // Ex: if myhost.example.org, subdomain will be: myhost
    $subdomain = substr($host, 0, $cut - 1);

    drush_log(dt("Symbiotic: DNS: Deleting record: %record", ['%record' => $subdomain]), 'ok');
    $command = "sudo /root/bin/{$config['provider']}-dns-delete-record.sh " . escapeshellarg($subdomain);
    $output = system($command);
    drush_log(dt("Symbiotic: DNS: Delete record result: %output", ['%output' => $output]), 'ok');
  }
}
