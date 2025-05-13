<?php

/**
 * Code moved from provision_symbiotic post_verify hook
 * so that it can be run independently using 'cv php:script'
 */

$root = $argv[1];
$host = $argv[2];
$site_path = $argv[3];
// @todo Probably not useful anymore (fetched from the Ansible inventory)
$hosting_restapi_token = $argv[4] ?? NULL;
$hosting_restapi_hostmaster = $argv[5] ?? NULL;

$output = [];

if (!$root || !$host || !$site_path) {
  throw new Exception("Missing arguments");
}

/**
 * Detects whether the URI pattern is for a dev site.
 * This is also used by hosting_civicrm_ansible for the icinga monitoring.
 */
function _provision_symbiotic_is_dev_site($host) {
  // Match against dev patterns:
  // - crm-dev.* (SymbioTIC)
  // - *.symbiodev.xyz (SymbioTIC)
  // - *.crm-dev.* (legacy)
  // - *.www-test.* (CiviCRM.org)
  // - *test*makoa.net (Makoa)
  // - test*-dms (CH)
  if (preg_match('/^crm-dev\./', $host)) {
    return TRUE;
  }
  if (preg_match('/symbiodev.xyz/', $host)) {
    return TRUE;
  }
  if (preg_match('/\.crm-dev\./', $host)) {
    return TRUE;
  }
  if (preg_match('/\.www-test\./', $host)) {
    return TRUE;
  }
  if (preg_match('/\.test\.makoa\.net/', $host)) {
    return TRUE;
  }
  if (preg_match('/\.dev\.makoa\.net/', $host)) {
    return TRUE;
  }
  if (preg_match('/test.*-dms/', $host)) {
    return TRUE;
  }

  return FALSE;
}

/**
 * Automatically sets Environment=Development and disables iATS logins on dev sites.
 *
 * NB: we disable by changing the login, since the iATS recurring Scheduled Job
 * will still run even if the payment processor is disabled. Since recent 5.x CiviCRM
 * releases, jobs are not run when Environment=Development, but we are still doing
 * this just in case someone accidentally changes the environment back to 'production'.
 */
function _provision_symbiotic_disable_jobs_on_devsites($host, &$output) {
  if (!_provision_symbiotic_is_dev_site($host)) {
    return;
  }

  $output[] = "Symbiotic: This is a dev site, setting environment=Development and checking if iATS needs to be disabled";

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
      $output[] = "Symbiotic: processor_id={$val['id']} credentials already renamed ({$val['user_name']})";
      continue;
    }

    if (preg_match('/iats/i', $val['class_name']) || preg_match('/eway/i', $val['class_name'])) {
      $new_user_name = $val['user_name'] . 'DISABLEDBYAEGIR';

      civicrm_api3('PaymentProcessor', 'create', [
        'id' => $val['id'],
        'user_name' => $new_user_name,
      ]);

      $output[] = "Symbiotic: {$val['class_name']} processor_id={$val['id']} credentials have been disabled (renamed login to: {$new_user_name})";
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

        $output[] = "Symbiotic: Disabled iATS Scheduled Job: {$val['name']}";
      }
    }
  }
}

// Make sure that CiviCRM path/url settings are correct.
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

_provision_symbiotic_disable_jobs_on_devsites($host, $output);

// Change the CiviCRM site_id, if necessary (ex: cloning from a template site)
$site_id = CRM_Utils_System::getSiteID();

if ($site_id == 'template' && strpos($host, 'template') === FALSE && strpos($host, 'modele') === FALSE) {
  $sid = md5('sid_' . time() . $host);
  civicrm_api3('Setting', 'create', ['domain_id' => 'all', 'site_id' => $sid]);
  Civi::settings()->set('hosting_restapi_initial_done', 0);
}

// The "verify" task can run regularly. Check to make sure we only run once,
// after the initial clone.
if (Civi::settings()->get('hosting_restapi_initial_done')) {
  $output[] = 'Symbiotic: hosting_restapi_initial_done already done, returning.';
  echo json_encode($output);
  return;
}

// Fetch the token from the Ansible inventory, because we are trying to avoid relying on Aegir
// the inventory is considered to be secret (it also has CiviCRM site keys, etc).
$hostmaster = Civi::settings()->get('hosting_restapi_hostmaster');
$result = file_get_contents($hostmaster . '/inventory');
$data = json_decode($result, TRUE);
$token = $data['_meta']['hostvars'][$host]['hosting_restapi_token'] ?? NULL;

// We can't auto-configure sites without a token.
// This includes the model site, the hostmaster, or sites not managed through hosting_restapi.
if (!$token) {
  $output[] = 'CiviCRM: no hosting_restapi_token, returning';
  echo json_encode($output);
  return;
}

// Not sure if necessary
Civi::settings()->set('hosting_restapi_token', $token);

// @todo Below has not been tested on Drupal9+ or other CMS
if (CIVICRM_UF != 'Drupal') {
  $output[] = 'Symbiotic: CMS is not Drupal7, auto-config not yet supported';
  echo json_encode($output);
  return;
}

// Request the site configuration
$result = file_get_contents($hostmaster . '/hosting/api/site/config?url=' . $host . '&token=' . $token);
$config = json_decode($result, TRUE);

if (!empty($config['status']) && $config['status'] == 'error') {
  $output[] = 'Symbiotic: warning: failed to fetch data about the site: ' . print_r($config, 1);
  echo json_encode($output);
  return;
}

if (function_exists('variable_set')) {
  variable_set('site_name', $config['data']['site']['name']);
}

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
  $output[] = 'Symbiotic: warning: failed to set some CiviCRM configurations: ' . $e->getMessage();
  echo json_encode($output);
  return;
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

  // Workaround weird new 2025-05 bug, where the hostname is empty
  if (!preg_match('/^http/', $onetime)) {
    $onetime = 'https://' . $_SERVER['HTTP_HOST'] . $onetime;
  }

  $result = drupal_http_request($hostmaster . '/hosting/api/site/welcome', [
    'method' => 'POST',
    'data' => [
      'url' => $host,
      'token' => $token,
      'loginurl' => $onetime,
    ],
    'headers' => ['Content-Type' => 'application/x-www-form-urlencoded'],
  ]);

  $output[] = 'Symbiotic: sending welcome one-time link: ' . $onetime . ' via ' . $hostmaster . ' == ' . print_r($result, 1);
}

echo json_encode($output);
