<?php

/**
 * Implementation of hook_drush_command().
 */
function provision_symbiotic_drush_command() {
  $items['provision-symbiotic-login'] = array(
    'description' => 'Generates a one-time link for user 2.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['provision-symbiotic-hush-alerts'] = array(
    'description' => 'Hide CiviCRM Status Alerts.',
  );
  $items['provision-symbiotic-role-permission'] = array(
    'description' => 'Add a permission to a role.',
    'arguments' => [
      'rid' => 'Role ID',
      'permission' => 'Permission',
    ],
  );
  $items['provision-symbiotic-translate-options'] = array(
    'description' => 'Translate Option Groups and Option Values in the default locale',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'arguments' => [
      'language' => 'Language, ex: fr_CA',
      'gid' => 'Option Group ID',
    ],
  );
  $items['provision-symbiotic-civicrm-stats'] = array(
    'description' => 'Returns the total number of CiviCRM contacts, activities, contributions, memberships, mailings, cases, events and participants',
  );
  $items['provision-symbiotic-last-login'] = array(
    'description' => 'Returns the last login time from a user (excluding uid=1).',
  );
  $items['provision-symbiotic-hostmaster-enable-https'] = array(
    'description' => 'Enables https for the Aegir hostmaster.',
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
    drupal_session_destroy_uid(2);
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
    $file = dirname(__FILE__) . '/tpl/custom-apache-vhost_ssl.tpl.php';
    return $file;
  }
  elseif (is_a($config, 'Provision_Config_Nginx_Ssl_Site')) {
    // c.f. #239, for IP vhost override (old hosting_ssl).
    $file = dirname(__FILE__) . '/tpl/custom-nginx-vhost_ssl.tpl.php';
    return $file;
  }
  elseif (is_a($config, 'Provision_Config_Nginx_Https_Site')) {
    // c.f. #239, for IP vhost override (hosting_https).
    $file = dirname(__FILE__) . '/tpl/custom-nginx-vhost_https.tpl.php';
    return $file;
  }
  elseif (is_a($config, 'Provision_Config_Nginx_Site')) {
    // To force binding IPv6 [::]
    $file = dirname(__FILE__) . '/tpl/custom-nginx-vhost.tpl.php';
    return $file;
  }
  elseif (is_a($config, 'Provision_Config_Nginx_Server')) {
    $file = dirname(__FILE__) . '/tpl/custom-nginx-server.tpl.php';
    return $file;
  }
  elseif (is_a($config, 'Provision_Config_Nginx_Ssl_Server')) {
    $file = dirname(__FILE__) . '/tpl/custom-nginx-server_ssl.tpl.php';
    return $file;
  }
  elseif (is_a($config, 'Provision_Config_Nginx_Https_Server')) {
    $file = dirname(__FILE__) . '/tpl/custom-nginx-server_ssl.tpl.php';
    return $file;
  }
  elseif (is_a($config, 'Provision_Config_Http_Inc_Server')) {
    $file = dirname(__FILE__) . '/tpl/custom-nginx-vhost_include.tpl.php';
    return $file;
  }
}

/**
 * Implements hook_drush_init().
 */
function provision_symbiotic_drush_init() {
  // Register our service classes for autoloading.
  provision_symbiotic_provision_register_autoload();
}

/**
 * Register our directory as a place to find Provision classes.
 *
 * This allows Provision to autoload our classes, so that we don't need to
 * specifically include the files before we use the class.
 */
function provision_symbiotic_provision_register_autoload() {
  static $loaded = FALSE;
  if (!$loaded) {
    $loaded = TRUE;
    $list = drush_commandfile_list();
    $provision_dir = dirname($list['provision']);
    include_once($provision_dir . '/provision.inc');
    include_once($provision_dir . '/provision.service.inc');
    provision_autoload_register_prefix('Provision_', dirname(__FILE__));
  }
}

/**
 * Expose the service type this extension defines to provision.
 *
 * @return
 *   An array with the service type the key, and the default implementation the value.
 */
function provision_symbiotic_provision_services() {
  provision_symbiotic_provision_register_autoload();
  return array(
    'symbiotic' => NULL,
  );
}

/**
 * Wrapper to initialize CiviCRM, depending on the CMS.
 *
 * Returns FALSE if this is not a CiviCRM site (ex: hostmaster).
 */
function provision_symbiotic_civicrm_init() {
  if (drush_drupal_major_version() == 8 && Drupal::hasService('civicrm')) {
    \Drupal::service('civicrm')->initialize();
    return TRUE;
  }
  elseif (function_exists('civicrm_initialize')) {
    civicrm_initialize();
    return TRUE;
  }

  return FALSE;
}

/**
 * Callback for provision-symbiotic-hush-alerts
 *
 * Hush CiviCRM alerts.
 */
function drush_provision_symbiotic_hush_alerts() {
  if (!provision_symbiotic_civicrm_init()) {
    return;
  }

  $hush = ['checkDirVariables', 'checkUrlVariables', 'checkPhpVersion', 'checkVersion', 'checkDirsWritable', 'checkIndices', 'checkPriceFields', 'extensionUpdates'];

  foreach ($hush as $h) {
    civicrm_api3('StatusPreference', 'create', [
      'name' => $h,
      'ignore_severity' => 'warning',
      'hush_until' => '',
    ]);
  }
}

/**
 * Callback for provision-symbiotic-role-permission
 *
 * Add a permission to a role.
 */
function drush_provision_symbiotic_role_permission($rid, $permission) {
  if (!provision_symbiotic_civicrm_init()) {
    return;
  }

  // Configurateur
  $permissions = [$permission];
  user_role_grant_permissions($rid, $permissions);
}

/**
 * Callback for provision-symbiotic-civicrm-stats
 *
 * Returns the total number of CiviCRM contacts, activities, contributions,
 * memberships, mailings, cases, events and event participants.
 */
function drush_provision_symbiotic_civicrm_stats() {
  if (!provision_symbiotic_civicrm_init()) {
    echo "0,0,0,0,0,0";
    return;
  }

  echo CRM_Core_DAO::singleValueQuery('SELECT count(*) FROM civicrm_contact WHERE is_deleted = 0') . ',';
  echo CRM_Core_DAO::singleValueQuery('SELECT count(*) FROM civicrm_activity WHERE is_test = 0 AND is_deleted = 0 AND is_current_revision = 1') . ',';
  echo CRM_Core_DAO::singleValueQuery('SELECT count(*) FROM civicrm_contribution WHERE is_test = 0') . ',';
  echo CRM_Core_DAO::singleValueQuery('SELECT count(*) FROM civicrm_membership WHERE is_test = 0') . ',';
  echo CRM_Core_DAO::singleValueQuery('SELECT count(*) FROM civicrm_mailing') . ',';
  echo CRM_Core_DAO::singleValueQuery('SELECT count(*) FROM civicrm_case WHERE is_deleted = 0') . ',';
  echo CRM_Core_DAO::singleValueQuery('SELECT count(*) FROM civicrm_event') . ',';
  echo CRM_Core_DAO::singleValueQuery('SELECT count(*) FROM civicrm_participant WHERE is_test = 0') . ',';

  // Fetch the payment processors used
  $processors = _provision_symbiotic_get_payment_processors();
  echo implode('+', $processors) . ',';

  // Fetch the last login
  $last = _provision_symbiotic_last_login();
  echo $last;
}

/**
 * Implements the provision-symbiotic-translate-options command.
 */
function drush_provision_symbiotic_translate_options($language, $option_group_id = NULL) {
  drush_bootstrap(DRUSH_BOOTSTRAP_DRUPAL_FULL);

  $entities = ['OptionValue', 'OptionGroup'];

  $params = [
    'sequential' => 1,
    'option.limit' => 0,
    'option.language' => $language,
  ];

  if ($option_group_id) {
    $params['option_group_id'] = $option_group_id;
  }

  foreach ($entities as $entity) {
    $results = civicrm_api3($entity, 'get', $params);

    foreach ($results['values'] as $option) {
      $ts_label = ts($option['label']);

      if ($option['label'] != $ts_label) {
        civicrm_api3('OptionValue', 'create', array(
          'id' => $option['id'],
          'label' => $ts_label,
        ));
      }
    }
  }
}

/**
 * Implements the provision-symbiotic-last-login command.
 *
 * Returns the date of last login from a non-superadmin.
 */
function drush_provision_symbiotic_last_login() {
  $last = _provision_symbiotic_last_login();
  drush_log('Last login: ' . $record->t, 'ok');
}

function _provision_symbiotic_last_login() {
  if (!provision_symbiotic_civicrm_init()) {
    return null;
  }

  $config = CRM_Core_Config::singleton();

  if ($config->userFramework == 'Drupal') {
    $last = db_query('SELECT date(from_unixtime(access)) as t from users where uid != 1 order by access desc limit 1')->fetchField();
    return $last;
  }
  elseif ($config->userFramework == 'Drupal8') {
    // TODO
    return null;
  }
  elseif ($config->userFramework == 'WordPress') {
    // TODO
    return null;
  }

  return null;
}

/**
 * Helper function that returns the list of payment processors enabled.
 */
function _provision_symbiotic_get_payment_processors() {
  if (!provision_symbiotic_civicrm_init()) {
    return null;
  }

  $processors = [];

  $result = civicrm_api3('PaymentProcessor', 'get', [
    'is_active' => 1,
    'is_test' => 0,
    'api.PaymentProcessorType.get' => [],
  ]);

  foreach ($result['values'] as $key => $val) {
    $type = $val['api.PaymentProcessorType.get']['values'][0]['name'];

    if ($type == 'Dummy') {
      continue;
    }

    $processors[] = $type;
  }

  return $processors;
}

/**
 * Implements the provision-symbiotic-hostmaster-enable-https
 */
function drush_provision_symbiotic_hostmaster_enable_https() {
  if (d()->name != '@hm') {
    drush_die('This command should only run on the Aegir hostmaster alias, i.e. @hm.');
  }

  // Find the nid of the web server node
  $nid_http = db_query("SELECT nid FROM hosting_service WHERE service = 'http' AND type = 'https_nginx'")->fetchField();

  // Find the nid of the hostmaster node
  // It should be the first site installed (and usually the only site at this point)
  $nid_hm = db_query("SELECT nid FROM hosting_site ORDER BY nid ASC")->fetchField();

  // Check if https has already been enabled
  $count = db_query('SELECT count(*) FROM hosting_https_server')->fetchField();

  if ($count) {
    drush_log(dt("https has already been enabled on this server."), 'warning');
  }
  else {
    // Find the web server node (i.e. the one that's not a database server)
    db_query("INSERT INTO hosting_https_server (nid, vid, https_port)
      select s.nid, s.vid, 443
        from hosting_server s
        left join node n on (n.nid = s.nid)
        left join hosting_db_server db on (db.nid = s.nid)
       where db.nid is null");

    // Enable https_nginx on that server
    db_query("update hosting_service set type = 'https_nginx' where service = 'http' and type = 'nginx'");

    // Enable LetsEncrypt
    db_query("insert into hosting_service (vid,nid,service,type,port,available) values (2,2,'Certificate','LetsEncrypt', 0, 1)");
    db_query("update hosting_https_site set https_enabled = 2 where nid = 10");

    drush_log(dt("https has been enabled on this server."), 'ok');
  }

  // Force Aegir to run its tasks that will eventually enable https
  $node = node_load($nid_http);
  node_save($node);

  // Same for the hostmaster node
  // FIXME: this might run before the previous task has finished running
  // FIXME: sometimes nginx needs a full restart for LE to work?
  $node = node_load($nid_hm);
  node_save($node);
}
