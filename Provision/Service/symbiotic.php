<?php

/**
 * The symbiotic service class.
 */
class Provision_Service_symbiotic extends Provision_Service {
  public $service = 'symbiotic';

  /**
   * Add the property to the site context.
   */
  static function subscribe_site($context) {
    $context->setProperty('hosting_restapi_token');
    $context->setProperty('hosting_restapi_hostmaster');
  }
}
