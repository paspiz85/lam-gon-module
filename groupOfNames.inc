<?php

use \LAM\TYPES\TypeManager;
use function LAM\TYPES\getScopeFromTypeId;
use LAM\TYPES\ConfiguredType;

/*

This code is part of LDAP Account Manager (http://www.ldap-account-manager.org/)
Copyright (C) 2003 - 2006  Tilo Lutz
              2007 - 2025  Roland Gruber
              2025 - 2025  Pasquale Pizzuti

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


/**
 * Manages the object class "groupOfNames" for groups.
 *
 * @package modules
 *
 * @author Pasquale Pizzuti
 */
class groupOfNames extends baseModule {

	/** cache for existing users and their GIDs */
	private $cachedUserToGIDList;
	/** cache for existing groups */
	private $cachedGroupNameList;

	/**
	 * {@inheritDoc}
	 * @see baseModule::getManagedAttributes()
	 */
	function get_uploadColumns($selectedModules, &$type) {
		$return = parent::get_uploadColumns($selectedModules, $type);
		$typeId = $type->getId();
		if ($this->manageCnAndDescription($selectedModules)) {
			array_unshift($return,
				[
					'name' => 'groupOfNames_cn',
					'description' => _('Group name'),
					'help' => 'cn',
					'example' => _('adminstrators'),
					'required' => true,
					'unique' => true
				]
			);
			array_unshift($return,
				[
					'name' => 'groupOfNames_description',
					'description' => _('Group description'),
					'help' => 'descriptionList',
					'example' => _('Administrators group')
				]
			);
		}
		if (!$this->isBooleanConfigOptionSet('groupOfNames_' . $typeId . '_hidemember')) {
			$return[] = [
				'name' => 'groupOfNames_members',
				'description' => _('Group members'),
				'help' => 'upload_members',
				'example' => _('user01,user02,user03')
			];
		}
		return $return;
	}

	/**
	 * {@inheritDoc}
	 * @see baseModule::build_uploadAccounts()
	 */
	function build_uploadAccounts($rawAccounts, $ids, &$partialAccounts, $selectedModules, &$type) {
		$error_messages = [];
		$typeId = $type->getId();
		for ($i = 0; $i < count($rawAccounts); $i++) {
			if (!in_array("groupOfNames", $partialAccounts[$i]['objectClass'])) {
				$partialAccounts[$i]['objectClass'][] = "groupOfNames";
			}
			if ($this->manageCnAndDescription($selectedModules)) {
				// group name
				$this->mapSimpleUploadField($rawAccounts, $ids, $partialAccounts, $i, 'groupOfNames_cn', 'cn', 'groupname', $this->messages['cn'][3], $error_messages);
			}
			if ($this->manageCnAndDescription($selectedModules)) {
				// description (UTF-8, no regex check needed)
				if ($rawAccounts[$i][$ids['groupOfNames_description']] == "") {
					$partialAccounts[$i]['description'] = $partialAccounts[$i]['cn'];
				}
				else {
					$this->mapSimpleUploadField($rawAccounts, $ids, $partialAccounts, $i, 'groupOfNames_description', 'description', null, null, $errors, '/;[ ]*/');
				}
			}
			// group members
			if (!$this->isBooleanConfigOptionSet('groupOfNames_' . $typeId . '_hidemember') && ($rawAccounts[$i][$ids['groupOfNames_members']] != "")) {
				if (get_preg($rawAccounts[$i][$ids['groupOfNames_members']], 'usernameList')) {
					$partialAccounts[$i]['member'] = explode(",", $rawAccounts[$i][$ids['groupOfNames_members']]);
				}
				else {
					$errMsg = $this->messages['member'][0];
					$errMsg[] = $i;
					$error_messages[] = $errMsg;
				}
			}
		}
		return $error_messages;
	}

	/**
	 * {@inheritDoc}
	 * @see baseModule::delete_attributes()
	 */
	function delete_attributes(): array {
		$return = [];
		/* TODO
		$result = searchLDAPByFilter('(&(objectClass=posixAccount)(gidNumber=' . $this->attributes['gidNumber'][0] . '))', ['dn'], ['user', 'host']);
		if (count($result) > 0) {
			$max = 5;
			if (count($result) < 5) {
				$max = count($result);
			}
			$users = [];
			for ($i = 0; $i < $max; $i++) {
				$users[] = getAbstractDN($result[$i]['dn']);
			}
			$message = $this->messages['primaryGroup'][0];
			$message[] = implode(', ', $users);
			$return[$this->getAccountContainer()->dn_orig]['errors'][] = $message;
		}
		*/
		return $return;
	}

	/**
	 * Returns the HTML meta data for the main account page.
	 *
	 * @return array HTML meta data
	 *
	 * @see baseModule::get_metaData()
	 */
	function display_html_attributes() {
		$return = new htmlResponsiveRow();
		$modules = $this->getAccountContainer()->get_type()->getModules();
		$typeId = $this->getAccountContainer()->get_type()->getId();
		if ($this->autoAddObjectClasses || (isset($this->attributes['objectClass']) && in_array('groupOfNames', $this->attributes['objectClass']))) {
			// group name
			if ($this->manageCnAndDescription($modules)) {
				$this->addSimpleInputTextField($return, 'cn', _("Group name"), true);
			}
			// description
			if ($this->manageCnAndDescription($modules)) {
				$this->addMultiValueInputTextField($return, 'description', _('Description'));
			}
			// group members
			if (!$this->isBooleanConfigOptionSet('groupOfNames_' . $typeId . '_hidemember')) {
				$return->addVerticalSpacer('0.5rem');
				$return->addLabel(new htmlOutputText(_("Group members")));
				$membersGroup = new htmlGroup();
				if (!$this->isBooleanConfigOptionSet('groupOfNames_autoSyncGon')) {
					$membersGroup->addElement(new htmlAccountPageButton(static::class, 'user', 'open', _('Edit members')));
				}
				$membersGroup->addElement(new htmlHelpLink('members'));
				$return->addField($membersGroup);
				$return->addLabel(new htmlOutputText('&nbsp;', false));
				$users = $this->getUsers();
				$members = [];
				if (isset($this->attributes['member'][0])) {
					foreach ($this->attributes['member'] as $member) {
						$members[] = (isset($users[$member]) && isset($users[$member]['cn'])) ? $users[$member]['uid'] . ' (' . $users[$member]['cn'] . ')' : $member;
					}
				}
				$members = array_unique($members);
				natcasesort($members);
				$members = array_map('htmlspecialchars', $members);
				$return->addField(new htmlOutputText(implode('<br>', $members), false));
			}
			// remove button
			if (!$this->autoAddObjectClasses) {
				$return->addVerticalSpacer('2rem');
				$remButton = new htmlButton('remObjectClass', _('Remove Unix extension'));
				$remButton->setCSSClasses(['lam-danger']);
				$return->add($remButton, 12, 12, 12, 'text-center');
			}
		}
		else {
			// add button
			$return->add(new htmlButton('addObjectClass', _('Add Unix extension')), 12);
		}
		return $return;
	}


	/**
	 * Displays selections to add or remove users from current group.
	 *
	 * @return array meta HTML output
	 */
	function display_html_user() {
		$return = new htmlResponsiveRow();
		if (!isset($this->attributes['member'])) {
			$this->attributes['member'] = [];
		}
		// load list with all users
		$userAndGIDs = $this->getUsers();
		$users = [];
		foreach ($userAndGIDs as $user => $userAttrs) {
			if (!in_array($user, $this->attributes['member'])) {
				$display = $userAttrs['uid'] . ' (' . $userAttrs['cn'] . ')';
				$users[$display] = $user;
			}
		}
		natcasesort($users);
		$return->add(new htmlSubTitle(_("Group members")), 12);

		$remUsers = [];
		if (isset($this->attributes['member'])) {
			$remUsers = $this->attributes['member'];
		}
		natcasesort($remUsers);
		$remUsersDescriptive = [];
		foreach ($remUsers as $user) {
			if (isset($userAndGIDs[$user])) {
				$remUsersDescriptive[$userAndGIDs[$user]['uid'] . ' (' . $userAndGIDs[$user]['cn'] . ')'] = $user;
			}
			else {
				//$remUsersDescriptive[$user] = $user;
			}
		}

		$this->addDoubleSelectionArea($return, _("Selected users"), _("Available users"), $remUsersDescriptive, null,
			$users, null, 'members', false, true);

		// back button
		$return->addVerticalSpacer('2rem');
		$return->add(new htmlAccountPageButton(static::class, 'attributes', 'back', _('Back')), 12);

		return $return;
	}

	/**
	 * Returns true if this module can manage accounts of the current type, otherwise false.
	 *
	 * @return boolean true if module fits
	 */
	public function can_manage() {
		return $this->get_scope() === 'group';
	}

	/**
	 * Returns meta data that is interpreted by parent class
	 *
	 * @return array array with meta data
	 */
	function get_metaData() {
		$return = [];
		// icon
		$return['icon'] = 'group.svg';
		if ($this->get_scope() == "group") {
			// this is a base module
			$return["is_base"] = true;
			// LDAP filter
			$return["ldap_filter"] = ['or' => "(objectClass=groupOfNames)"];
		}
		// alias name
		$return["alias"] = _('Group');
		// RDN attribute
		$return["RDN"] = ["cn" => "normal"];
		// module dependencies
		$return['dependencies'] = ['depends' => [], 'conflicts' => []];
		// managed object classes
		$return['objectClasses'] = ['groupOfNames'];
		// LDAP aliases
		$return['LDAPaliases'] = ['commonName' => 'cn'];
		// managed attributes
		$return['attributes'] = ['member'];
		// profile options
		if (!$this->autoAddObjectClasses) {
			$profileContainer = new htmlResponsiveRow();
			$profileContainer->add(new htmlResponsiveInputCheckbox('groupOfNames_addExt', false, _('Automatically add this extension'), 'autoAdd'), 12);
			$return['profile_options'] = $profileContainer;
		}
		// available PDF fields
		$return['PDF_fields'] = [];
		// upload fields
		$return['upload_columns'] = [];
		// help Entries
		$return['help'] = [
			'description' => [
				"Headline" => _("Description"), 'attr' => 'description',
				"Text" => _("Group description. If left empty group name will be used.")
			],
			'descriptionList' => [
				"Headline" => _("Description"), 'attr' => 'description',
				"Text" => _("Group description. If left empty group name will be used.") . ' ' . _("Multiple values are separated by semicolon.")
			],
			'members' => [
				"Headline" => _("Group members"), 'attr' => 'member',
				"Text" => _("Users who are member of the current group. Users who have set their primary group to this group will not be shown.")
			],
			'upload_members' => [
				"Headline" => _("Group members"), 'attr' => 'member',
				"Text" => _("Users who will become member of the current group. User names are separated by semicolons.")
			],
			'minMaxGID' => [
				"Headline" => _("GID number"),
				"Text" => _("These are the minimum and maximum numbers to use for group IDs when creating new group accounts. New group accounts will always get the highest number in use plus one.")
			],
			'pwdHash' => [
				"Headline" => _("Password hash type"),
				"Text" => _("LAM supports a large number of possibilities to generate the hash value of passwords. CRYPT-SHA512 and SSHA are the most common. We do not recommend to use plain text passwords unless passwords are hashed server-side.")
			],
			'cn' => [
				"Headline" => _("Group name"), 'attr' => 'cn',
				"Text" => _("Group name of the group which should be created. Valid characters are: a-z, A-Z, 0-9 and .-_ . If group name is already used group name will be expanded with a number. The next free number will be used.")
			],
			'gidCheckSuffix' => [
				"Headline" => _("Suffix for GID/group name check"),
				"Text" => _("LAM checks if the entered group name and GID are unique. Here you can enter the LDAP suffix that is used to search for duplicates. By default, the account type suffix is used. You only need to change this if you use multiple server profiles with different OUs but need unique group names or GIDs.")
			],
			'gidGenerator' => [
				"Headline" => _("GID generator"),
				"Text" => _("LAM will automatically suggest UID/GID numbers. You can either use a fixed range of numbers or an LDAP entry with object class \"sambaUnixIdPool\" or \"msSFU30DomainInfo\".")
					. ' ' . _('Magic number will set a fixed value that must match your server configuration.')
			],
			'sambaIDPoolDN' => [
				"Headline" => _("Samba ID pool DN"),
				"Text" => _("Please enter the DN of the LDAP entry with object class \"sambaUnixIdPool\".")
			],
			'windowsIDPoolDN' => [
				"Headline" => _("Windows domain info DN"),
				"Text" => _("Please enter the DN of the LDAP entry with object class \"msSFU30DomainInfo\".")
			],
			'filter' => [
				"Headline" => _("Filter"),
				"Text" => _("Here you can enter a filter value. Only entries which contain the filter text will be shown.")
					. ' ' . _('The filter can be any regular expression, e.g. ".*" = any characters, "^" = line start, "$" = line end.')
			],
			'hidemember' => [
				"Headline" => _('Disable membership management'), 'attr' => 'member',
				"Text" => _('Disables the group membership management.')
			],
			'autoAdd' => [
				"Headline" => _("Automatically add this extension"),
				"Text" => _("This will enable the extension automatically if this profile is loaded.")
			],
			'magicNumber' => [
				"Headline" => _("Magic number"),
				"Text" => _("Please enter the magic number you configured on server side.")
			],
		];

		return $return;
	}


	/**
	 * {@inheritDoc}
	 * @see baseModule::get_configOptions()
	 */
	public function get_configOptions($scopes, $allScopes) {
		$typeManager = new TypeManager($_SESSION['conf_config']);
		// configuration options
		$configContainer = new htmlResponsiveRow();
		$configContainer->add(new htmlSubTitle(_("Groups")), 12);
		foreach ($allScopes[static::class] as $typeId) {
			if (count($allScopes[static::class]) > 1) {
				$title = new htmlDiv(null, new htmlOutputText($typeManager->getConfiguredType($typeId)->getAlias()));
				$title->setCSSClasses(['bold', 'responsiveLabel']);
				$configContainer->add($title, 12, 6);
				$configContainer->add(new htmlOutputText('&nbsp;', false), 0, 6);
			}
			$this->addAccountSpecificConfigOptions($configContainer, $typeId);
			$configContainer->addVerticalSpacer('2rem');
		}
		return $configContainer;
	}

	/**
	 * Adds the account specific options to the config container.
	 *
	 * @param htmlResponsiveRow $configContainer container
	 * @param string $typeId type ID
	 */
	protected function addAccountSpecificConfigOptions(&$configContainer, $typeId) {
		$genOptions = [
			_('Fixed range') => 'range',
			_('Samba ID pool') => 'sambaPool',
			_('Windows domain info') => 'windowsDomain',
			_('Magic number') => 'magicNumber'
		];
		$gidGeneratorSelect = new htmlResponsiveSelect('groupOfNames_' . $typeId . '_gidGenerator', $genOptions, ['range'], _('GID generator'), 'gidGenerator');
		$gidGeneratorSelect->setHasDescriptiveElements(true);
		$gidGeneratorSelect->setTableRowsToHide([
			'range' => ['groupOfNames_' . $typeId . '_sambaIDPoolDN', 'groupOfNames_' . $typeId . '_windowsIDPoolDN', 'groupOfNames_' . $typeId . '_magicNumber'],
			'sambaPool' => ['groupOfNames_' . $typeId . '_minGID', 'groupOfNames_' . $typeId . '_maxGID', 'groupOfNames_' . $typeId . '_windowsIDPoolDN', 'groupOfNames_' . $typeId . '_magicNumber'],
			'windowsDomain' => ['groupOfNames_' . $typeId . '_minGID', 'groupOfNames_' . $typeId . '_maxGID', 'groupOfNames_' . $typeId . '_sambaIDPoolDN', 'groupOfNames_' . $typeId . '_magicNumber'],
			'magicNumber' => ['groupOfNames_' . $typeId . '_minGID', 'groupOfNames_' . $typeId . '_maxGID', 'groupOfNames_' . $typeId . '_windowsIDPoolDN', 'groupOfNames_' . $typeId . '_sambaIDPoolDN']
		]);
		$gidGeneratorSelect->setTableRowsToShow([
			'range' => ['groupOfNames_' . $typeId . '_minGID', 'groupOfNames_' . $typeId . '_maxGID'],
			'sambaPool' => ['groupOfNames_' . $typeId . '_sambaIDPoolDN'],
			'windowsDomain' => ['groupOfNames_' . $typeId . '_windowsIDPoolDN'],
			'magicNumber' => ['groupOfNames_' . $typeId . '_magicNumber']
		]);
		$configContainer->add($gidGeneratorSelect, 12);
		$minGidInput = new htmlResponsiveInputField(_('Minimum GID number'), 'groupOfNames_' . $typeId . '_minGID', null, 'minMaxGID');
		$minGidInput->setRequired(true);
		$configContainer->add($minGidInput, 12);
		$maxGidInput = new htmlResponsiveInputField(_('Maximum GID number'), 'groupOfNames_' . $typeId . '_maxGID', null, 'minMaxGID');
		$maxGidInput->setRequired(true);
		$configContainer->add($maxGidInput, 12);
		$gidGeneratorDN = new htmlResponsiveInputField(_('Samba ID pool DN'), 'groupOfNames_' . $typeId . '_sambaIDPoolDN', null, 'sambaIDPoolDN');
		$gidGeneratorDN->setRequired(true);
		$configContainer->add($gidGeneratorDN, 12);
		$winGeneratorDN = new htmlResponsiveInputField(_('Windows domain info DN'), 'groupOfNames_' . $typeId . '_windowsIDPoolDN', null, 'windowsIDPoolDN');
		$winGeneratorDN->setRequired(true);
		$configContainer->add($winGeneratorDN, 12);
		$magicNumber = new htmlResponsiveInputField(_('Magic number'), 'groupOfNames_' . $typeId . '_magicNumber', null, 'magicNumber');
		$magicNumber->setRequired(true);
		$configContainer->add($magicNumber, 12);
		$configContainer->add(new htmlResponsiveInputField(_('Suffix for GID/group name check'), 'groupOfNames_' . $typeId . '_gidCheckSuffix', '', 'gidCheckSuffix'), 12);
		$configContainer->add(new htmlResponsiveInputCheckbox('groupOfNames_' . $typeId . '_hidemember', false, _('Disable membership management'), 'hidemember'), 12);
	}

	/**
	 * {@inheritDoc}
	 * @see baseModule::check_configOptions()
	 */
	public function check_configOptions($typeIds, &$options) {
		foreach ($typeIds as $typeId) {
		}
		return parent::check_configOptions($typeIds, $options);
	}

	/**
	 * {@inheritDoc}
	 * @see baseModule::get_pdfFields()
	 */
	public function get_pdfFields($typeId) {
		$fields = parent::get_pdfFields($typeId);
		$typeManager = new TypeManager();
		$modules = $typeManager->getConfiguredType($typeId)->getModules();
		if ($this->manageCnAndDescription($modules)) {
			$fields['cn'] = _('Group name');
			$fields['description'] = _('Description');
		}
		if (!$this->isBooleanConfigOptionSet('groupOfNames_' . $typeId . '_hidemember')) {
			$fields['member'] = _('Group members');
			$fields['memberPrimary'] = _('Group members (incl. primary members)');
		}
		return $fields;
	}

	/**
	 * {@inheritDoc}
	 * @see baseModule::get_pdfEntries()
	 */
	function get_pdfEntries($pdfKeys, $typeId) {
		$return = [];
		$this->addSimplePDFField($return, 'member', _('Group members'));
		$this->addSimplePDFField($return, 'cn', _('Group name'));
		$this->addSimplePDFField($return, 'description', _('Description'));
		if (in_array(static::class . '_memberPrimary', $pdfKeys)) {
			$members = empty($this->attributes['member']) ? [] : $this->attributes['member'];
			$this->addPDFKeyValue($return, 'memberPrimary', _('Group members'), $members);
		}
		return $return;
	}


	/**
	 * This function will be called when the module will be loaded
	 *
	 * @param String $base the name of the {@link accountContainer} object ($_SESSION[$base])
	 */
	function init($base) {
		// call parent init
		parent::init($base);
	}


	/**
	 * This function fills the $messages variable with output messages from this module.
	 */
	function load_Messages() {
		$this->messages['userPassword'][1] = ['ERROR', _('Password'), _('Password contains invalid characters. Valid characters are:') . ' a-z, A-Z, 0-9 and #*,.;:_-+!%&/|?{[()]}=@$ §°!'];
		$this->messages['cn'][0] = ['WARN', _('Group name'), _('You are using capital letters. This can cause problems because Windows is not case-sensitive.')];
		$this->messages['cn'][1] = ['WARN', _('Group name'), _('Group name in use. Selected next free group name.')];
		$this->messages['cn'][2] = ['ERROR', _('Group name'), _('Group name contains invalid characters. Valid characters are: a-z, A-Z, 0-9 and .-_ !')];
		$this->messages['cn'][3] = ['ERROR', _('Account %s:') . ' groupOfNames_cn', _('Group name contains invalid characters. Valid characters are: a-z, A-Z, 0-9 and .-_ !')];
		$this->messages['member'][0] = ['ERROR', _('Account %s:') . ' groupOfNames_members', _("This value must be a list of user names separated by semicolons.")];
		$this->messages['primaryGroup'][0] = ['ERROR', _('There are still users who have this group as their primary group.')];
		$this->messages['sambaIDPoolDN'][0] = ['ERROR', _('Samba ID pool DN'), _('This is not a valid DN!')];
		$this->messages['windowsIDPoolDN'][0] = ['ERROR', _('Windows domain info DN'), _('This is not a valid DN!')];
		$this->messages['magicNumber'][0] = ['ERROR', _('Magic number'), _('Please enter a valid number.')];
	}


	/**
	 * {@inheritDoc}
	 * @see baseModule::getManagedAttributes()
	 */
	public function getManagedAttributes($typeId) {
		$attrs = parent::getManagedAttributes($typeId);
		$typeManager = new TypeManager();
		$modules = $typeManager->getConfiguredType($typeId)->getModules();
		if ($this->manageCnAndDescription($modules)) {
			$attrs[] = 'cn';
			$attrs[] = 'description';
		}
		return $attrs;
	}

	/**
	 * This functions is used to check if all settings for this module have been made.
	 *
	 * @return boolean true, if settings are complete
	 */
	function module_complete() {
		if (!$this->getAccountContainer()->isNewAccount) {
			// check if account is based on our object class
			$objectClasses = $this->getAccountContainer()->attributes_orig['objectClass'];
			if (is_array($objectClasses) && !in_array('groupOfNames', $objectClasses)) {
				return true;
			}
		}
		$modules = $this->getAccountContainer()->get_type()->getModules();
		if ($this->manageCnAndDescription($modules) && ($this->attributes['cn'][0] == '')) {
			return false;
		}
		return true;
	}


	/**
	 * Controls if the module button the account page is visible and activated.
	 *
	 * @return string status ("enabled", "disabled", "hidden")
	 */
	function getButtonStatus() {
		if (!$this->getAccountContainer()->isNewAccount) {
			// check if account is based on our object class
			$objectClasses = $this->getAccountContainer()->attributes_orig['objectClass'];
			if (is_array($objectClasses) && !in_array('groupOfNames', $objectClasses)) {
				return "disabled";
			}
		}
		return "enabled";
	}


	/**
	 * Processes user input of the primary module page.
	 * It checks if all input values are correct and updates the associated LDAP attributes.
	 *
	 * @return array list of info/error messages
	 */
	function process_attributes() {
		$errors = [];
		if (isset($_POST['addObjectClass'])) {
			if (!isset($this->attributes['objectClass'])) {
				$this->attributes['objectClass'] = [];
			}
			if (!in_array('groupOfNames', $this->attributes['objectClass'])) {
				$this->attributes['objectClass'][] = 'groupOfNames';
			}
			return $errors;
		}
		if (isset($_POST['remObjectClass'])) {
			$this->attributes['objectClass'] = array_delete(['groupOfNames'], $this->attributes['objectClass']);
			$attrs = $this->getManagedAttributes($this->getAccountContainer()->get_type()->getId());
			foreach ($attrs as $name) {
				if (isset($this->attributes[$name])) {
					unset($this->attributes[$name]);
				}
			}
			return $errors;
		}
		$modules = $this->getAccountContainer()->get_type()->getModules();
		$typeId = $this->getAccountContainer()->get_type()->getId();
		// skip processing if object class is not set
		if (!$this->autoAddObjectClasses && (!isset($this->attributes['objectClass']) || !in_array('groupOfNames', $this->attributes['objectClass']))) {
			return $errors;
		}
		if ($this->manageCnAndDescription($modules)) {
			$this->processMultiValueInputTextField('description', $errors);
		}
		if ($this->manageCnAndDescription($modules)) {
			$this->attributes['cn'][0] = $_POST['cn'];
			if (preg_match('/^[A-Z]+$/', $_POST['cn'])) {
				$errors[] = $this->messages['cn'][0];
			}
			// Check if Groupname contains only valid characters
			if (!get_preg($this->attributes['cn'][0], 'groupname')) {
				$errors[] = $this->messages['cn'][2];
			}
			// Create automatic useraccount with number if original user already exists
			// Reset name to original name if new name is in use
			// Set username back to original name if new group name is in use
			if ($this->groupNameExists($this->attributes['cn'][0]) && ($this->orig['cn'][0] != '')) {
				$this->attributes['cn'][0] = $this->orig['cn'][0];
			}
			// Change gid to a new gid until a free gid is found
			else {
				while ($this->groupNameExists($this->attributes['cn'][0])) {
					// get last character of group name
					$lastchar = substr($this->attributes['cn'][0], strlen($this->attributes['cn'][0]) - 1, 1);
					// Last character is no number
					if (!preg_match('/^(\d)+$/', $lastchar)) {
						/* Last character is no number. Therefore we only have to
						* add "2" to it.
						*/
						$this->attributes['cn'][0] .= '2';
					}
					else {
						/* Last character is a number -> we have to increase the number until we've
						* found a groupname with trailing number which is not in use.
						*
						* $i will show us were we have to split groupname so we get a part
						* with the groupname and a part with the trailing number
						*/
						$i = strlen($this->attributes['cn'][0]) - 1;
						// Set $i to the last character which is a number in $account_new->general_username
						while (true) {
							if (preg_match('/^(\d)+$/', substr($this->attributes['cn'][0], $i, strlen($this->attributes['cn'][0]) - $i))) {
								$i--;
							}
							else {
								break;
							}
						}
						// increase last number with one
						$firstchars = substr($this->attributes['cn'][0], 0, $i + 1);
						$lastchars = substr($this->attributes['cn'][0], $i + 1, strlen($this->attributes['cn'][0]) - $i);
						// Put username together
						$this->attributes['cn'][0] = $firstchars . (intval($lastchars) + 1);
					}
				}
			}
			// Show warning if lam has changed group name
			if ($this->attributes['cn'][0] != $_POST['cn']) {
				$errors[] = $this->messages['cn'][1];
			}
		}
		// Return error-messages
		return $errors;
	}


	/**
	 * Processes user input of the user selection page.
	 * It checks if all input values are correct and updates the associated LDAP attributes.
	 *
	 * @return array list of info/error messages
	 */
	function process_user() {
		$return = [];
		if (!isset($this->attributes['member'])) {
			$this->attributes['member'] = [];
		}
		if (isset($_POST['members_2']) && isset($_POST['members_left'])) { // Add groups to list
			// add new group
			$this->attributes['member'] = @array_merge($this->attributes['member'], $_POST['members_2']);
		}
		elseif (isset($_POST['members_1']) && isset($_POST['members_right'])) { // remove groups from list
			$this->attributes['member'] = array_delete($_POST['members_1'], $this->attributes['member']);
		}
		return $return;
	}

	/**
	 * Returns a list of modifications which have to be made to the LDAP account.
	 *
	 * @return array list of modifications
	 * <br>This function returns an array with 3 entries:
	 * <br>array( DN1 ('add' => array($attr), 'remove' => array($attr), 'modify' => array($attr)), DN2 .... )
	 * <br>DN is the DN to change. It may be possible to change several DNs (e.g. create a new user and add him to some groups via attribute member)
	 * <br>"add" are attributes which have to be added to LDAP entry
	 * <br>"remove" are attributes which have to be removed from LDAP entry
	 * <br>"modify" are attributes which have to been modified in LDAP entry
	 * <br>"info" are values with informational value (e.g. to be used later by pre/postModify actions)
	 */
	function save_attributes() {
		// skip saving if account is based on another structural object class
		if ($this->is_base_module() && !$this->getAccountContainer()->isNewAccount && !in_array('groupOfNames', $this->getAccountContainer()->attributes_orig['objectClass'])) {
			return [];
		}
		if (!in_array('groupOfNames', $this->attributes['objectClass']) && !in_array('groupOfNames', $this->orig['objectClass'])) {
			// skip saving if the extension was not added/modified
			return [];
		}
		$return = $this->getAccountContainer()->save_module_attributes($this->attributes, $this->orig);
		return $return;
	}

	/**
	 * Loads the values of an account profile into internal variables.
	 *
	 * @param array $profile hash array with profile values (identifier => value)
	 */
	function load_profile($profile) {
		// profile mappings in meta data
		parent::load_profile($profile);
		// add extension
		if (isset($profile['groupOfNames_addExt'][0]) && ($profile['groupOfNames_addExt'][0] == "true")
			&& !in_array('groupOfNames', $this->attributes['objectClass'])) {
			$this->attributes['objectClass'][] = 'groupOfNames';
		}
	}

	/**
	 * This method specifies if a module manages password attributes.
	 * @return boolean true if this module manages password attributes
	 * @see passwordService::managesPasswordAttributes
	 *
	 */
	public function managesPasswordAttributes() {
		return true;
	}

	/**
	 * Specifies if this module supports to force that a user must change his password on next login.
	 *
	 * @return boolean force password change supported
	 */
	public function supportsForcePasswordChange() {
		return false;
	}

	/**
	 * Returns a list of existing users and their GID numbers and cn.
	 *
	 * @return array list in format array(uid => array('gid' => 123, 'cn' => 'Some user'))
	 */
	protected function getUsers() {
		if ($this->cachedUserToGIDList != null) {
			return $this->cachedUserToGIDList;
		}
		$this->cachedUserToGIDList = [];
		$typeManager = new TypeManager();
		foreach ($typeManager->getConfiguredTypesForScope('user') as $type) {
			$filter = '(&(objectClass=inetOrgPerson)(uid=*))';
			if ($this->isWindows()) {
				$filter = '(&(objectClass=user)(uid=*))';
			}
			$result = searchLDAPByFilter($filter, ['uid', 'cn'], ['user']);
			$resultCount = count($result);
			for ($i = 0; $i < $resultCount; $i++) {
				$this->cachedUserToGIDList[$result[$i]['dn']] = [
					'cn' => $result[$i]['cn'][0],
					'uid' => $result[$i]['uid'][0],
					'dn' => $result[$i]['dn']];
			}
		}
		logNewMessage(LOG_DEBUG, 'Found ' . $resultCount . ' users.');
		return $this->cachedUserToGIDList;
	}

	/**
	 * Checks if the given group name already exists in LDAP.
	 *
	 * @param String $groupName group name
	 * @return boolean true if already exists
	 */
	private function groupNameExists($groupName) {
		return in_array($groupName, $this->getGroupNames());
	}

	/**
	 * Returns a list of all group names in LDAP.
	 *
	 * @return array group names
	 */
	private function getGroupNames() {
		if ($this->cachedGroupNameList != null) {
			return $this->cachedGroupNameList;
		}
		$this->cachedGroupNameList = [];
		$attrs = ['cn'];
		$filter = '(&(objectClass=groupOfNames)(cn=*))';
		$suffix = $this->getAccountContainer()->get_type()->getSuffix();
		$typeId = $this->getAccountContainer()->get_type()->getId();
		if (!empty($this->moduleSettings['groupOfNames_' . $typeId . '_gidCheckSuffix'][0])) {
			$suffix = $this->moduleSettings['groupOfNames_' . $typeId . '_gidCheckSuffix'][0];
		}
		$result = searchLDAP($suffix, $filter, $attrs);
		for ($i = 0; $i < count($result); $i++) {
			$this->cachedGroupNameList[] = $result[$i]['cn'][0];
		}
		return $this->cachedGroupNameList;
	}

	/**
	 * Returns if the Windows module is active.
	 *
	 * @return boolean is Windows
	 */
	private function isWindows() {
		return (in_array('windowsGroup', $this->getAccountContainer()->get_type()->getModules()));
	}

	/**
	 * Returns if cn and description attributes should be managed.
	 *
	 * @param string[] $modules modules
	 * @return boolean manage cn+description
	 */
	protected function manageCnAndDescription($modules) {
		return true;
	}

	/**
	 * @inheritDoc
	 */
	public function supportsPasswordQuickChangePage(): bool {
		return false;
	}

	/**
	 * @inheritDoc
	 */
	public function addPasswordQuickChangeAccountDetails(htmlResponsiveRow $row): void {
		// no details
	}

	/**
	 * @inheritDoc
	 */
	public function getPasswordQuickChangeOptions(): array {
		return [];
	}

	/**
	 * @inheritDoc
	 */
	public function getPasswordQuickChangeChanges(string $password): array {
		return [];
	}

	/**
	 * @inheritDoc
	 */
	public function getPasswordQuickChangePasswordStrengthUserName(): ?string {
		return null;
	}

	/**
	 * @inheritDoc
	 */
	public function getPasswordQuickChangePasswordStrengthAttributes(): array {
		return [];
	}

	/**
	 * @inheritDoc
	 */
	public function getPasswordQuickChangeIsPasswordInHistory(string $password): bool {
		return false;
	}

	/**
	 * @inheritDoc
	 */
	public function getListAttributeDescriptions(ConfiguredType $type): array {
		return [
			"cn" => _("Name"),
			"description" => _("Group description"),
			"member" => _("Group members"),
		];
	}

}
