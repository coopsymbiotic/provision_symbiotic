<?php

/**
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

  return $htaccess;
}

/**
 * Implements hook_post_provision_verify().
 */
function drush_provision_symbiotic_post_provision_verify($url = NULL) {
  if (d()->type == 'site') {
    // Make sure that CiviCRM path/url settings are correct.
    $root = d()->root;
    $host = d()->uri;

    Civi::settings()->set('userFrameworkResourceURL', "https://$host/sites/all/modules/civicrm");
    Civi::settings()->set('uploadDir', "$root/sites/$host/files/civicrm/upload/");
    Civi::settings()->set('customFileUploadDir', "$root/sites/$host/files/civicrm/custom/");

    Civi::settings()->set('customPHPPathDir', NULL);
    Civi::settings()->set('customTemplateDir', NULL);

    Civi::settings()->set('extensionsDir', "$root/sites/$host/modules/extensions/");
    Civi::settings()->set('extensionsURL', "https://$host/sites/$host/modules/extensions");

    Civi::settings()->set('imageUploadDir', "$root/sites/$host/files/civicrm/persist/contribute");
    Civi::settings()->set('imageUploadURL', "https://$host/sites/$host/files/civicrm/persist/contribute");

    // The "verify" task can run regularly. Check to make sure we only run once,
    // after the initial clone.
    if (variable_get('hosting_restapi_initial_done', NULL)) {
      return;
    }

    // We can't auto-configure sites without a token.
    // This includes the model site, the hostmaster, or sites not managed through hosting_restapi.
    if (! d()->hosting_restapi_token) {
      return;
    }

    variable_set('hosting_restapi_token', d()->hosting_restapi_token);
    variable_set('hosting_restapi_hostmaster', d()->hosting_restapi_hostmaster);

    // Request the site configuration
    $result = drupal_http_request(d()->hosting_restapi_hostmaster . '/sites/config?url=' . d()->uri . '&token=' . d()->hosting_restapi_token);
    $config = json_decode($result->data);

    civicrm_initialize();

    variable_set('site_name', $config['organization']['name']);
    variable_set('site_mail', $config['organization']['email']);

    try {
      civicrm_api3('Contact', 'Create', array(
        'id' => 1,
        'organization_name' => $config['organization']['name'],
        'display_name' => $config['organization']['name'],
        'phone' => $config['organization']['phone'],
        'email' => $config['organization']['email'],
      ));

/*
      civicrm_api3('Phone', 'Create', array(
        'contact_id' => 1,
        'phone' => $config['organization']['phone'],
      ));
*/

      // TODO: individual contact + create new user account & notify?
      // however.. we can't notify & include the password by email
      // and if the notify includes a one-time link, it would expire from the login-reset task (if it can work on uid=2).
    }
    catch (Exception $e) {
      drush_log('Symbiotic: failed to set some CiviCRM configurations: ' . $e->getMessage(), 'warning');
    }

    variable_set('hosting_restapi_initial_done', 1);
  }
}
