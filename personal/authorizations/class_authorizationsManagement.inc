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
	var $write_memberUid = false;
	var $write_authorizedService = false;


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

		/* Load ACLs */
		$this->write_memberUid = strstr($this->getAcl('memberUid'), 'w') !== false;
		$this->write_authorizedService = strstr($this->getAcl('authorizedService'), 'w') !== false;
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


	/**
	 * Get all the authorizations from the configuration
	 */
	private function getAuthorizations() {
		return $this->getAuthorizationsRecurse($this->auth_config->root->children);
	}

	private function getAuthorizationsRecurse($children) {
		$result = array();

		foreach($children as $child) {
			if(self::ELT_CAT === $child->name) {
				$result = array_merge($result, $this->getAuthorizationsRecurse($child->children));
			} else if(self::ELT_AUT === $child->name) {
				$result[] = $child;
			}
		}

		return $result;
	}

	/**
	 * Returns true if the user has the authorization
	 */
	private function userHasAuthorization($authorization) {
		// check if the user has the authorized service attribute
		if(isset($authorization->attributes['service'])) {
			$count = isset($this->attrs['authorizedService']['count']) ? $this->attrs['authorizedService']['count'] : 0;

			for($i = 0; $i < $count; ++$i) {
				if($authorization->attributes['service'] === $this->attrs['authorizedService'][$i]) {
					return true;
				}
			}
			
			return false;
		} else if(isset($authorization->attributes['group'])) {
			if(count($this->groups)) {
				foreach($this->groups as $dn => $cn) {
					if($cn === $authorization->attributes['group']) {
						return true;
					}
				}
			}

			return false;
		} else {
			return false;
		}
	}


	/**
	 * Generate the html for this node
	 */
	public function execute($isCopyPaste = false) {
		parent::execute();
		$display = null;

		if(!file_exists(CONFIG_DIR . DIRECTORY_SEPARATOR . self::CONF_FILE)) {
			$display .= '<img alt="" src="images/small-error.png" align="middle">&nbsp;';
			$display .= '<b>' . _('The configuration file doesn\'t exist') . '</b>';
			$display .= back_to_main();
			return $display;
		} else if(!is_readable(CONFIG_DIR . DIRECTORY_SEPARATOR . self::CONF_FILE)) {
			$display .= '<img alt="" src="images/small-error.png" align="middle">&nbsp;';
			$display .= '<b>' . _('The configuration file isn\'t readable') . '</b><br />';
			$display .= back_to_main();
			return $display;
		}

		/* Load configuration */
		if(null === $this->auth_config) {
			try {
				$this->auth_config = $this->loadConfig(CONFIG_DIR . DIRECTORY_SEPARATOR . self::CONF_FILE);
			} catch(Exception $e) {
				$display .= '<img alt="" src="images/small-error.png" align="middle">&nbsp;';
				$display .= '<b>' . _('Error while loading the configuration: ') . '</b>';
				$display .= '<ul>';
				
				foreach($e->getErrors() as $error) {
					$display .= '<li>' . $error->file . ':' . $error->line . ':' . $error->column . ': ' . $error->message . '</li>';
				}

				$display .= '</ul>';
				$display .= back_to_main();
				return $display;				
			}
		}

		$display .= $this->display($this->auth_config->root->children);

		return $display;
	}


	/**
	 * Transform the configuration to html
	 */
	private function display($children) {
		$display  = '';

		foreach($children as $child) {
			if(self::ELT_CAT === $child->name) {
				$display .= '<fieldset>' . PHP_EOL;
				$display .= '  <legend>' . $child->attributes['label'] . '</legend>' . PHP_EOL;
				$display .= $this->display($child->children);
				$display .= '</fieldset>' . PHP_EOL;
			} else if(self::ELT_AUT === $child->name) {
				$checked = '';

				if($this->userHasAuthorization($child)) {
					$checked = 'checked ';
				}

				$disabled = '';

				if(isset($child->attributes['group']) && !$this->write_memberUid) {
					$disabled = ' disabled="disabled"';
				}

				if(isset($child->attributes['service']) && !$this->write_authorizedService) {
					$disabled = ' disabled="disabled"';
				}

				$display .= '<input type="checkbox" id="' . $child->attributes['id'] . '" name="' . $child->attributes['id'] . '" value="1" ' . $checked . $disabled . '/>' . PHP_EOL;
				$display .= '<label for="' . $child->attributes['id'] . '">' . $child->attributes['label'] . '</label><br />' . PHP_EOL;
			} else {
				// unknown type... error ?
			}
		}

		return $display;
	}


	/**
	 * Check if all required groups exist
	 */
	public function check() {
		$messages = parent::check();
		$authorizations = $this->getAuthorizations();

		$ldap = null;

		foreach($authorizations as $authorization) {
			if(isset($_POST[$authorization->attributes['id']]) && $_POST[$authorization->attributes['id']] === '1') {
				if(isset($authorization->attributes['group'])) {
					if(null === $ldap) {
						$ldap = $this->config->get_ldap_link();
					}

					$ldap->cd($this->config->current['BASE']);
					$ldap->search('(&(objectClass=posixGroup)(cn=' . $authorization->attributes['group'] . '))', array('cn'));
					$attributes = $ldap->fetch();

					if(0 === count($attributes)) {
						$messages[] = _('The group') . ' ' . $authorization->attributes['group'] . ' ' . _('doesn\'t exist');
					}
				}
			}
		}

		return $messages;
	}


	/**
	 * Reorder the arrays keys : if array is [3] => 'value1', [4] => 'value2'
	 * the result will be [0] => 'value1', [1] => 'value2'
	 */
	private function c14nArray(array $in) {
		$out = array();

		foreach($in as $item) {
			$out[] = $item;
		}

		return $out;
	}


	/**
	 * Remove a value from a array
	 */
	private function arrayRemove(array $haystack, $value, $strict = false) {
		$value_idx = array_search($value, $haystack, $strict);

		if(false !== $value_idx) {
			unset($haystack[$value_idx]);
		}
		
		return $this->c14nArray($haystack);
	}


	/**
	 * Save the authorizations for the current user
	 */
	public function save() {
		$authorizations = $this->getAuthorizations();
		$edit = array();
		$groups = array();
		$ldap = null;

		foreach($authorizations as $authorization) {
			$to_be_enabled = isset($_POST[$authorization->attributes['id']]) && $_POST[$authorization->attributes['id']] === '1';
			$is_enabled = $this->userHasAuthorization($authorization);

			if($is_enabled !== $to_be_enabled) {
				if(isset($authorization->attributes['service'])) {
					if(!$this->write_authorizedService) {
						continue;
					}

					/* Update the authorizedService attribute */
					$service = $authorization->attributes['service'];

					if(!isset($edit['authorizedService'])) {
						if(isset($this->attrs['authorizedService'])) {
							$edit['authorizedService'] = $this->attrs['authorizedService'];
							unset($edit['authorizedService']['count']);
						} else {
							$edit['authorizedService'] = array();
						}
					}
					
					if($to_be_enabled) {
						/* Add authorizedServiceObject objectClass if required */
						for($i = 0; !$has_objectClass && $i < $this->attrs['objectClass']['count']; ++$i) {
							if('authorizedServiceObject' === $this->attrs['objectClass'][$i]) {
								$has_objectClass = true;
							}
						}

						if(!$has_objectClass) {
							$edit['objectClass'] = $this->attrs['objectClass'];
							unset($edit['objectClass']['count']);
							$edit['objectClass'][] = 'authorizedServiceObject';
							$has_objectClass = true;
						}

						/* Add the service name into the authorizedService attribute */
						$edit['authorizedService'][] = $service;
					} else {
						/* Remove the service name from the authorizedService attribute */
						$edit['authorizedService'] = $this->arrayRemove($edit['authorizedService'], $service);
					}
				} else if(isset($authorization->attributes['group'])) {
					if(!$this->write_memberUid) {
						continue;
					}

					/* Update group memberUid attribute */
					$group = $authorization->attributes['group'];

					if(null === $ldap) {
						$ldap = $this->config->get_ldap_link();
					}

					$ldap->cd($this->config->current['BASE']);
					$ldap->search('(&(objectClass=posixGroup)(cn=' . $group . '))', array('memberUid'));
					$attributes = $ldap->fetch();

					$groups[$attributes['dn']] = $attributes['memberUid'];
					unset($groups[$attributes['dn']]['count']);

					if($to_be_enabled) {
						/* Add the current user into the group */
						$groups[$attributes['dn']][] = $this->uid;
					} else {
						/* Remove the user name from the group */
						$groups[$attributes['dn']] = $this->arrayRemove($groups[$attributes['dn']], $this->uid);
					}
				}
			}
		}

		if(count($edit) > 0) {
			/* Performs the LDAP user modifications if required */
			if(null === $ldap) {
				$ldap = $this->config->get_ldap_link();
			}

			$ldap->cd($this->dn);
			$ldap->modify($edit);

			if (!$ldap->success()){
				msg_dialog::display(_("LDAP error"), msgPool::ldaperror($ldap->get_error(), $this->dn, LDAP_MOD, get_class()));
			}
		}

		if(count($groups) > 0) {
			/* Performs the LDAP posix group modifications if required */
			if(null === $ldap) {
				$ldap = $this->config->get_ldap_link();
			}

			foreach($groups as $dn => $memberUid) {
				$ldap->cd($dn);
				$ldap->modify(array('memberUid' => $memberUid));

				if (!$ldap->success()){
					msg_dialog::display(_("LDAP error"), msgPool::ldaperror($ldap->get_error(), $this->dn, LDAP_MOD, get_class()));
				}
			}
		}

		parent::save();
		$this->cleanup();
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
