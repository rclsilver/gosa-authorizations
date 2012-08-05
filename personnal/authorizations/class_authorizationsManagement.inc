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


	/**
	 * Load the plugin specific configuration
	 */
	private function loadConfig($filename) {
		libxml_use_internal_errors(true);

		$dom = new DomDocument();
		$res = $dom->load($filename);

		if(!$res) {
			$errors = libxml_get_errors();
			libxml_clear_errors();
			throw new XmlException($errors);
		}
		
		$config = new stdClass();
		$config->filename = $filename;

		$root = $dom->documentElement;
		$config->root = new stdClass();

		// recursive function to load the tree
		$this->loadConfigTree($root, $config->root);

		return $config;
	}


	/**
	 * Browse the tree
	 */
	private function loadConfigTree($dom_element, $object_element) {
		$object_element->name = $dom_element->nodeName;

		if($dom_element->hasAttributes()) {
			$object_element->attributes = array();

			foreach($dom_element->attributes as $key => $attribute) {
				$object_element->attributes[$key] = $attribute->value;
			}
		}

		if($dom_element->childNodes->length > 1) { /* more than 1 because first child is always textnode */
			$object_element->children = array();

			foreach($dom_element->childNodes as $dom_child) {
				if($dom_child->nodeType === XML_ELEMENT_NODE) {
					$child_object = new stdClass();
					$this->loadConfigTree($dom_child, $child_object);
					array_push($object_element->children, $child_object);
				}
			}
		}
	}
}

class XmlException extends Exception {
	var $errors;

	function __construct($xml_errors) {
		$this->errors = $xml_errors;
	}

	public function getErrors() {
		return $this->errors;
	}
}
