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
    'description' => 'Hide active CiviCRM Status Alerts.',
    'options' => [
      'severity' => 'The severity of active alerts to ignore. Ex: info, warning, critical. Defaults to warning.',
      'name' => 'The name of the check to ignore.',
    ],
    'aliases' => ['civihush'],
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
  $items['provision-symbiotic-civicrm-stats-aegir'] = array(
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'description' => 'Prints to stdout CiviCRM stats for the aegir-weekly script',
  );
  $items['provision-symbiotic-civicrm-stats'] = array(
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'description' => '(deprecated) Returns the total number of CiviCRM contacts, activities, contributions, memberships, mailings, cases, events and participants',
  );
  $items['provision-symbiotic-last-login'] = array(
    'description' => 'Returns the last login time from a user (excluding uid=1).',
  );
  $items['provision-symbiotic-cleanup'] = array(
    'description' => 'Runs database and potentially other cleanup operations (called from aegir-weekly).',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['provision-symbiotic-hostmaster-enable-https'] = array(
    'description' => 'Enables https for the Aegir hostmaster.',
  );
  $items['provision-symbiotic-list-stripe-webhooks'] = array(
    'description' => 'List Stripe webhooks (helps for 6.3.2 upgrades).',
    'options' => [
      'filter' => 'Filter domain',
    ],
  );
  $items['provision-symbiotic-delete-stripe-webhooks'] = array(
    'description' => 'Deletes Stripe webhooks (helps for 6.3.2 upgrades).',
    'options' => [
      'filter' => 'Filter domain',
    ],
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
  if (drush_drupal_major_version() != 7) {
    drush_log('drush_provision_symbiotic_login: Drupal 8 and above are not yet supported', 'warning');
    return;
  }

  // Assuming Drupal7
  $account = user_load(2);

  if (empty($account)) {
    return drush_set_error('PROVISION_UNABLE_TO_LOAD_UID_2', 'Could not load the admin user with uid 2 on this site.');
  }

  $onetime = user_pass_reset_url($account) . '/login';
  $onetime = preg_replace('/http:/', 'https:', $onetime);

  // pass the login link to the front end
  drush_set_option('symbiotic_login', $onetime);
  drush_log(dt('Login url: !onetime', array('!onetime' => $onetime)), 'success');

  drupal_session_destroy_uid(2);
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
    $file = dirname(__FILE__) . '/tpl/custom-apache-vhost_ssl.tpl.php';
    return $file;
  }
  elseif (is_a($config, 'Provision_Config_Nginx_Ssl_Site')) {
    $file = dirname(__FILE__) . '/tpl/custom-nginx-vhost_ssl.tpl.php';
    return $file;
  }
  elseif (is_a($config, 'Provision_Config_Nginx_Https_Site')) {
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
  if (drush_drupal_major_version() >= 8 && Drupal::hasService('civicrm')) {
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
 * Connects to the database using MySQLi from PHP, not the DB later from the CMS.
 * Used to simplify code shared on WordPress and Drupal(s).
 */
function provision_symbiotic_db_connect() {
  if (empty($_SERVER['db_host'])) {
    require_once d()->site_path . '/drushrc.php';
  }

  $db = new mysqli($_SERVER['db_host'], $_SERVER['db_user'], $_SERVER['db_passwd'], $_SERVER['db_name']);
  return $db;
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

  // The function param does not seem to work, perhaps because it's an old drush version
  $severity = 'warning';
  if ($option = drush_get_option('severity')) {
    $severity = $option;
  }

  $hush = ['checkDirVariables', 'checkUrlVariables', 'checkPhpVersion', 'checkVersion', 'checkDirsWritable', 'checkIndices', 'checkPriceFields', 'extensionUpdates', 'checkVersion_upgrade', 'checkExtensionsUpdates'];
  if ($option = drush_get_option('name')) {
    $hush = explode(',', $option);
  }

  foreach ($hush as $h) {
    civicrm_api3('StatusPreference', 'create', [
      'name' => $h,
      'ignore_severity' => $severity,
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
 * Callback for provision-symbiotic-civicrm-stats-aegir
 *
 * Prints the total number of CiviCRM contacts, activities, contributions,
 * memberships, mailings, cases, events and event participants.. in the
 * format expected by the aegir-weekly script (which in turn, in read by
 * the hosting_usage Aegir module to display stats to Aegir admins).
 */
function drush_provision_symbiotic_civicrm_stats_aegir() {
  $db = provision_symbiotic_db_connect();

  if ($db->connect_errno) {
    return;
  }

  $line_prefix = 'aegir,' . d()->uri . ',';

  try {
    echo $line_prefix . 'Users:' . $db->query('SELECT count(*) as x FROM users WHERE uid > 1')->fetch_assoc()['x'] . "\n";
    echo $line_prefix . 'CiviContact:' . $db->query('SELECT count(*) as x FROM civicrm_contact WHERE is_deleted = 0')->fetch_assoc()['x'] . "\n";
    echo $line_prefix . 'CiviActivity:' . $db->query('SELECT count(*) as x FROM civicrm_activity WHERE is_test = 0 AND is_deleted = 0 AND is_current_revision = 1')->fetch_assoc()['x'] . "\n";
    echo $line_prefix . 'CiviContribute:' . $db->query('SELECT count(*) as x FROM civicrm_contribution WHERE is_test = 0')->fetch_assoc()['x'] . "\n";
    echo $line_prefix . 'CiviMember:' . $db->query('SELECT count(*) as x FROM civicrm_membership WHERE is_test = 0')->fetch_assoc()['x'] . "\n";
    echo $line_prefix . 'CiviMail:' . $db->query('SELECT count(*) as x FROM civicrm_mailing WHERE is_completed=1 AND sms_provider_id IS NULL')->fetch_assoc()['x'] . "\n";
    echo $line_prefix . 'CiviSms:' . $db->query('SELECT count(*) as x FROM civicrm_mailing WHERE is_completed=1 AND sms_provider_id IS NOT NULL')->fetch_assoc()['x'] . "\n";
    echo $line_prefix . 'CiviCase:' . $db->query('SELECT count(*) as x FROM civicrm_case WHERE is_deleted = 0')->fetch_assoc()['x'] . "\n";
    echo $line_prefix . 'CiviEvent:' . $db->query('SELECT count(*) as x FROM civicrm_event')->fetch_assoc()['x'] . "\n";
    echo $line_prefix . 'CiviParticipant:' . $db->query('SELECT count(*) as x FROM civicrm_participant WHERE is_test = 0')->fetch_assoc()['x'] . "\n";
  }
  catch (Exception $e) {
    // Probably not a CiviCRM site
    return;
  }

  // Fetch the payment processors used
  $processors = _provision_symbiotic_get_payment_processors();
  echo $line_prefix . 'CiviPaymentProcessors:' . implode('+', $processors) . "\n";

  // Fetch the available languages
  // Depending on if multilingual or not, one of these should be valid
  $result = $db->query("SELECT value FROM civicrm_setting WHERE name IN ('uiLanguages','languageLimit') and value is not null");

  if ($result) {
    $record = $result->fetch_assoc();

    if (!empty($record['value'])) {
      $langs = unserialize($record['value']);
      $languages = implode('+', array_keys($langs));
      echo $line_prefix . 'CiviLanguages:' . $languages . "\n";
    }
  }

  // Fetch the last login
  $last = _provision_symbiotic_last_login();
  echo $line_prefix . 'LastLogin:' . $last . "\n";
}

/**
 * Callback for provision-symbiotic-civicrm-stats (deprecated)
 *
 * Returns the total number of CiviCRM contacts, activities, contributions,
 * memberships, mailings, cases, events and event participants.
 */
function drush_provision_symbiotic_civicrm_stats() {
  $db = provision_symbiotic_db_connect();

  if ($db->connect_errno) {
    echo "0,0,0,0,0,0";
    return;
  }

  try {
    echo $db->query('SELECT count(*) as x FROM civicrm_contact WHERE is_deleted = 0')->fetch_assoc()['x'] . ',';
    echo $db->query('SELECT count(*) as x FROM civicrm_activity WHERE is_test = 0 AND is_deleted = 0 AND is_current_revision = 1')->fetch_assoc()['x'] . ',';
    echo $db->query('SELECT count(*) as x FROM civicrm_contribution WHERE is_test = 0')->fetch_assoc()['x'] . ',';
    echo $db->query('SELECT count(*) as x FROM civicrm_membership WHERE is_test = 0')->fetch_assoc()['x'] . ',';
    echo $db->query('SELECT count(*) as x FROM civicrm_mailing WHERE is_completed=1 AND sms_provider_id IS NULL')->fetch_assoc()['x'] . ',';
    echo $db->query('SELECT count(*) as x FROM civicrm_mailing WHERE is_completed=1 AND sms_provider_id IS NOT NULL')->fetch_assoc()['x'] . ',';
    echo $db->query('SELECT count(*) as x FROM civicrm_case WHERE is_deleted = 0')->fetch_assoc()['x'] . ',';
    echo $db->query('SELECT count(*) as x FROM civicrm_event')->fetch_assoc()['x'] . ',';
    echo $db->query('SELECT count(*) as x FROM civicrm_participant WHERE is_test = 0')->fetch_assoc()['x'] . ',';
  }
  catch (Exception $e) {
    // Probably not a CiviCRM site
    echo "0,0,0,0,0,0";
    return;
  }

  // Fetch the payment processors used
  $processors = _provision_symbiotic_get_payment_processors();
  echo implode('+', $processors) . ',';

  // Fetch the available languages
  // Depending on if multilingual or not, one of these should be valid
  $result = $db->query("SELECT value FROM civicrm_setting WHERE name IN ('uiLanguages','languageLimit') and value is not null");

  if ($result) {
    $record = $result->fetch_assoc();

    if (!empty($record['value'])) {
      $langs = unserialize($record['value']);
      $languages = implode('+', array_keys($langs));
      echo $languages;
    }
  }

  echo ',';

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
  drush_log('Last login: ' . $last, 'ok');
}

function _provision_symbiotic_last_login() {
  $db = provision_symbiotic_db_connect();

  if ($db->connect_errno) {
    return '';
  }

  $cms = _provision_symbiotic_get_cms();

  try {
    if ($cms == 'Drupal9' || $cms == 'Drupal8') {
      $result = $db->query('SELECT date(from_unixtime(access)) as t from users_field_data where uid != 1 order by access desc limit 1');
      if ($result) {
        $record = $result->fetch_assoc();
        return $record['t'];
      }
    }
    elseif ($cms == 'Drupal7') {
      $result = $db->query('SELECT date(from_unixtime(access)) as t from users where uid != 1 order by access desc limit 1');
      if ($result) {
        $record = $result->fetch_assoc();
        return $record['t'];
      }
    }
    // @todo WordPress
  }
  catch (Exception $e) {
    // TODO
    return '';
  }

  return '';
}

/**
 * Implements the provision-symbiotic-cleanup command.
 *
 * Runs database (and potentially other) cleanup operations.
 * Used by /usr/local/bin/aegir-weekly.
 */
function drush_provision_symbiotic_cleanup() {
  // Find usage stats before cleanup
  $before = _provision_symbiotic_get_db_stats();
  drush_log('TOTAL Disk usage [before]: ' . $before['total']['disk_used'] . ' MB, reserved space: ' . $before['total']['disk_free'] . ' MB', 'ok');

  $db = provision_symbiotic_db_connect();

  if ($db->connect_errno) {
    return $stats;
  }

  // Start by deleting potentially large log tables
  try {
    // Keep only 10-14 days for certain logs
    // https://github.com/lcdservices/biz.lcdservices.joblogmanagement/blob/master/api/v3/JobLog/Purge.php
    $db->query('DELETE FROM civicrm_job_log WHERE run_time < (NOW() - INTERVAL 10 DAY)');
    // These logging tables can be huge and not particularly relevant
    $db->query('DELETE FROM log_civicrm_mailing WHERE log_date < (NOW() - INTERVAL 14 DAY)');
    $db->query('DELETE FROM log_civicrm_group WHERE log_date < (NOW() - INTERVAL 14 DAY)');
    $db->query('DELETE FROM log_civicrm_activity WHERE log_date < (NOW() - INTERVAL 30 DAY)');
    // These settings get updated often and are not useful
    $db->query('DELETE FROM log_civicrm_setting WHERE name IN ("navigation", "resCacheCode")');
  }
  catch (Exception $e) {
    // Probably not a CiviCRM site
  }

  try {
    $db->query('TRUNCATE watchdog');
  }
  catch (Exception $e) {
    // Probably not a Drupal site, or watchdog not enabled
  }

  try {
  }
  catch (Exception $e) {
    // Probably not a CiviCRM site, or detailed logging not enabled
  }

  // Optimize fragmented tables
  foreach ($before['tables'] as $key => $stat) {
    drush_log('Fragmented table [before]: ' . $key . ': ' . $stat['disk_used'] . ' MB, reserved space: ' . $stat['disk_free'] . ' MB', 'warning');
    $db->query('OPTIMIZE table ' . $key);
  }

  // Show stats after
  $after = _provision_symbiotic_get_db_stats();
  drush_log('TOTAL Disk usage [after]: ' . $after['total']['disk_used'] . ' MB, reserved space: ' . $after['total']['disk_free'] . ' MB', 'ok');

  foreach ($after['tables'] as $key => $stat) {
    drush_log('Fragmented table [after]: ' . $key . ': ' . $stat['disk_used'] . ' MB, reserved space: ' . $stat['disk_free'] . ' MB', 'warning');
  }

  // @todo Consider truncating specific cache tables
  // but doing so cause problems on sites. Maybe sometimes, if they get over a certain size?
  // - civicrm_cache
  // - form_cache
  // - sessions

  // @todo And then more complicated tables, maybe should use CiviCRM extensions instead
  // - civicrm_mailing_event_queue
  // - civicrm_log
  // - civicrm_activity (remove HTML from old messages)
}

function _provision_symbiotic_get_db_stats() {
  $stats = [
    'tables' => [],
    'total' => [],
  ];

  $db = provision_symbiotic_db_connect();

  if ($db->connect_errno) {
    return $stats;
  }

  $result = $db->query("SELECT
    round(sum(data_length + index_length)/1024/1024, 2) disk_used,
    round(sum(data_free)/ 1024 / 1024, 2) disk_free
    FROM `information_schema`.`TABLES`
    WHERE `TABLE_SCHEMA` <> 'information_schema'");

  $record = $result->fetch_object();

  $stats['total'] = [
    'disk_used' => $record->disk_used,
    'disk_free' => $record->disk_free,
  ];

  // Fragmented tables
  $result = $db->query("SELECT table_schema as db_name, table_name,
    round(sum(data_length + index_length)/1024/1024, 2) disk_used,
    round(sum(data_free)/ 1024 / 1024, 2) disk_free
    FROM `information_schema`.`TABLES`
    WHERE TABLE_SCHEMA <> 'information_schema'
    GROUP BY table_schema, table_name
    HAVING SUM(data_free) > 0 ORDER BY data_free DESC");

  while ($record = $result->fetch_object()) {
    $stats['tables'][$record->db_name . '.' . $record->table_name] = [
      'disk_used' => $record->disk_used,
      'disk_free' => $record->disk_free,
    ];
  }

  return $stats;
}

/**
 * Helper function that returns the list of payment processors enabled.
 */
function _provision_symbiotic_get_payment_processors() {
  $db = provision_symbiotic_db_connect();

  if ($db->connect_errno) {
    return null;
  }

  $processors = [];
  $result = $db->query("select class_name from civicrm_payment_processor where is_active = 1 and is_test = 0 and class_name != 'Payment_Dummy'");

  while ($record = $result->fetch_assoc()) {
    $processors[] = $record['class_name'];
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

/**
 * Helper function to get Stripe webhooks
 */
function _provision_symbiotic_get_stripe_webhooks($filter = '') {
  $return = [];
  provision_symbiotic_civicrm_init();

  foreach ([false, true] as $is_test) {
    $result = civicrm_api3('PaymentProcessor', 'get', [
      'class_name' => 'Payment_Stripe',
      'is_active' => 1,
      'is_test' => $is_test,
      'domain_id' => CRM_Core_Config::domainID(),
    ]);

    foreach ($result['values'] as $paymentProcessor) {
      $paymentProcessorId = $paymentProcessor['id'];
      $processor = new CRM_Core_Payment_Stripe('', civicrm_api3('PaymentProcessor', 'getsingle', ['id' => $paymentProcessorId]));
      $processor->setAPIParams();
      $webhooks = \Stripe\WebhookEndpoint::all(["limit" => 100]);

      foreach ($webhooks->data as $wh) {
        if ($filter) {
          if (strpos($wh['url'], $filter) !== FALSE) {
            $return[] = $wh;
          }
        }
        else {
          $return[] = $wh;
        }
      }
    }
  }

  return $return;
}

/**
 * Implements the provision-symbiotic-list-stripe-webhooks
 */
function drush_provision_symbiotic_list_stripe_webhooks() {
  $filter = drush_get_option('filter');
  $webhooks = _provision_symbiotic_get_stripe_webhooks($filter);

  foreach ($webhooks as $wh) {
    drush_log(print_r($wh, 1), 'ok');
  }
}

/**
 * Implements the provision-symbiotic-delete-stripe-webhooks
 */
function drush_provision_symbiotic_delete_stripe_webhooks() {
  $filter = drush_get_option('filter');

  if (!$filter) {
    drush_die('Please provide --filter=example.org to avoid deleting unrelated webhooks.');
  }

  $webhooks = _provision_symbiotic_get_stripe_webhooks($filter);

  foreach ($webhooks as $wh) {
    drush_log('Deleting webhook: ' . print_r($wh, 1), 'ok');
    $wh->delete();
  }
}

/**
 * Detects whether the URI pattern is for a dev site.
 * This is also used by hosting_civicrm_ansible for the icinga monitoring.
 */
function provision_symbiotic_is_dev_site($uri) {
  // Match against dev patterns:
  // - crm-dev.* (SymbioTIC)
  // - *.symbiodev.xyz (SymbioTIC)
  // - *.crm-dev.* (legacy)
  // - *.www-test.* (CiviCRM.org)
  // - *test*makoa.net (Makoa)
  // - test*-dms (CH)
  if (preg_match('/^crm-dev\./', $uri)) {
    return TRUE;
  }
  if (preg_match('/symbiodev.xyz/', $uri)) {
    return TRUE;
  }
  if (preg_match('/\.crm-dev\./', $uri)) {
    return TRUE;
  }
  if (preg_match('/\.www-test\./', $uri)) {
    return TRUE;
  }
  if (preg_match('/test.*makoa.net/', $uri)) {
    return TRUE;
  }
  if (preg_match('/test.*-dms/', $uri)) {
    return TRUE;
  }

  return FALSE;
}

function _provision_symbiotic_get_cms() {
  $db = provision_symbiotic_db_connect();

  if ($db->connect_errno) {
    return null;
  }

  // Drupal8 or 9 or 10, we identify as 9
  $result = $db->query("SHOW TABLES LIKE 'users_field_data'");
  if ($result && mysqli_num_rows($result) > 0) {
    return 'Drupal9';
  }

  $result = $db->query("SHOW TABLES LIKE 'users'");
  if ($result && mysqli_num_rows($result) > 0) {
    return 'Drupal7';
  }

  $result = $db->query("SHOW TABLES LIKE 'wp_options'");
  if ($result && mysqli_num_rows($result) > 0) {
    return 'WordPress';
  }

  return null;
}
