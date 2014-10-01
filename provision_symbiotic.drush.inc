<?php

/**
 * Specify a different template for rendering a config file.
 *
 * @param $config
 *   The Provision_config object trying to find its template.
 *
 * @return
 *   A filename of a template to use for rendering.
 *
 * @see hook_provision_config_load_templates_alter()
 */
function provision_symbiotic_provision_config_load_templates($config) {
  if (is_a($config, 'Provision_Config_Apache_Ssl_Site')) {
    // c.f. #239, for IP vhost override.
    $file = dirname(__FILE__) . '/tpl/custom-vhost_ssl.tpl.php';
    return $file;
  }
}
