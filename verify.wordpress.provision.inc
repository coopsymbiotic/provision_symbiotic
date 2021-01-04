<?php

/**
 * Implements hook_post_provision_wordpress_verify().
 */
function drush_provision_symbiotic_post_provision_wordpress_verify($url = NULL) {
  if (d()->type === 'wpsite') {
    if (function_exists('civicrm_initialize')) {
      // Change the CiviCRM site_id, if necessary (ex: cloning from a template site)
      $site_id = CRM_Utils_System::getSiteID();

      if ($site_id == 'template' && strpos($host, 'template') === FALSE && strpos($host, 'modele') === FALSE) {
        $sid = md5('sid_' . time() . $host);
        civicrm_api3('Setting', 'create', ['domain_id' => 'all', 'site_id' => $sid]);

        // Force a flush of the gettingStarted dashlet
        // to avoid persistent errors caused by an invalid site_key
        CRM_Core_DAO::executeQuery('DELETE FROM civicrm_cache where path like "dashboard_gettingStarted%"');
      }
    }
  }
}
