<?php

/**
 * Implementation of hook_drush_command().
 */
function provision_symbiotic_drush_command() {
  $items['provision-symbiotic-login'] = array(
    'description' => 'Generates a one-time link for user 2.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  return $items;
}


/**
 * Implements the provision-symbiotic-login command.
 *
 * Mostly copies /usr/share/drush/commands/provision/platform/reset.login.provision.inc
 * but for uid = 2.
 */
function drush_provision_symbiotic_login() {
  drush_bootstrap(DRUSH_BOOTSTRAP_DRUPAL_FULL);
  // Drupal 7/8 take a uid as an integer and not an array
  if (drush_drupal_major_version() >= 7) {
    $account = user_load(2);
  }
  else {
    $account = user_load(array('uid' => 2));
  }

  if (empty($account)) {
    return drush_set_error('PROVISION_UNABLE_TO_LOAD_UID_2', 'Could not load the admin user with uid 2 on this site.');
  }

  $onetime = user_pass_reset_url($account) . '/login';

  // pass the login link to the front end
  drush_set_option('symbiotic_login', $onetime);
  drush_log(dt('Login url: !onetime', array('!onetime' => $onetime)), 'success');

  if (drush_drupal_major_version() >= 7) {
    drupal_session_destroy_uid(1);
  }
  else {
    // valid for d5 and d6
    sess_destroy_uid(2);
  }
}

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
