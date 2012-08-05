<?php

class authorizationsManagement extends plugin {
	/* Definitions */
	var $plHeadline = 'Authorizations';
	var $plDescription = 'Edit users authorizations';


	/* Plugin specific values */
	const CONF_FILE = 'authorizations.xml';
	const ELT_CAT = 'category';
	const ELT_AUT = 'authorization';
	var $auth_config = null;
	var $groups = array();


	/**
	 * Plugin class constructor
	 */
	function authorizationsManagement(&$config, $dn = NULL, $parent = NULL) {
		/* Store the configuration */
		$this->config = $config;

		/* Load bases attributes */
		parent::plugin($config, $dn, $parent);

		/* Setting uid to default */
		if(isset($this->attrs['uid'][0])){
			$this->uid = $this->attrs['uid'][0];
		}

		if(isset($this->attrs['gidNumber'][0])){
			$this->gidNumber = $this->attrs['gidNumber'][0];
		}

		/* Load group membership for the current user */
		$ldap = $this->config->get_ldap_link();
		$ldap->cd($this->config->current['BASE']);
		$ldap->search('(&(objectClass=posixGroup)(|(memberUid=' . $this->uid . ')(gidNumber=' . $this->gidNumber . ')))', array('cn'));

		while($attributes = $ldap->fetch()) {
			$this->groups[$attributes['dn']] = $attributes['cn'][0];
		}
	}
}
