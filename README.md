Provision SymbioTIC
===================

Custom provision submodule for configurations used by Coop SymbioTIC.

Includes:

* Block xmlrpc.php access
* Initial SaaS configuration on verify (if a token was found)
* Always bind vhosts `*`, even for https (therefore using SNI), so that we can also easily support IPv6.
* Nginx strict SSL configurations (thanks ouaibe/duraconf)
* Various drush commands to help with site management at Symbiotic. Includes `provision-symbiotic-civicrm-stats`, which is called by our fork of aegir-weekly.sh.

This module overrides the vhost template (c.f. 'tpl' directory) for Apache and Nginx.

You can find the latest version of this module at:  
https://github.com/coopsymbiotic/provision_symbiotic

See also
========

* https://github.com/ouaibe/duraconf/tree/master/configs/nginx
* https://github.com/coopsymbiotic/httpstools

Support
=======

Please use the github issue queue for community support:  
https://github.com/coopsymbiotic/provision_symbiotic/issues

Commercial support available from Coop Symbiotic:  
https://www.symbiotic.coop

Copyright
=========

Copyright (C) 2013-2020 Mathieu Lutfy (mathieu@symbiotic.coop)

License: GPL v3 or later. http://www.gnu.org/copyleft/gpl.html
