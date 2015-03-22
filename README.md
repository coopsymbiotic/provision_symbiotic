Provision SymbioTIC
===================

Custom provision submodule for configurations used by Coop SymbioTIC.

Includes:

* Block xmlrpc.php access
* Initial SaaS configuration on verify (if a token was found)
* Always bind vhosts '*', even for https (therefore using SNI), so that we can also easily support IPv6.
* Nginx only (for now): strict SSL configurations (thanks ouaibe/duraconf)

This module overrides the vhost template (c.f. 'tpl' directory) for Apache and Nginx.

You can find the latest version of this module at:  
https://github.com/coopsymbiotic/provision_symbiotic

NB: To support OCSP stapling, you must provide the certificate chain in
the same directory as the SSL key/cert.

For example:

  /var/aegir/config/ssl.d/example.org/openssl_chain.crt

See also
========

* https://github.com/ouaibe/duraconf/tree/master/configs/nginx
* https://github.com/mlutfy/provision_sts
* https://github.com/coopsymbiotic/httpstools

Support
=======

Please use the github issue queue for community support:  
https://github.com/coopsymbiotic/provision_symbiotic/issues

Commercial support available from Coop Symbiotic:  
https://www.symbiotic.coop

Copyright
=========

Copyright (C) 2013-2015 Mathieu Lutfy (mathieu@bidon.ca)

License: GPL v3 or later. http://www.gnu.org/copyleft/gpl.html
