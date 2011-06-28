<?php
/**
 * svid_leden module: administrative functions.
 *
 * Copyright 2010 Studievereniging i.d
 * Author: Raymond Jelierse
 *
 * Provided as-is.
 */

function svid_leden_import_form() {
  $form = array();
  $form['#attributes']['enctype'] = 'multipart/form-data';
  // File selector
  $form['import_file'] = array(
    '#title' => t('Source'),
    '#type' => 'file',
  );
  $form['file_settings'] = array(
    '#title' => t('File settings'),
    '#type' => 'fieldset',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE
  );
  $form['file_settings']['delimiter'] = array(
    '#title' => t('Delimiter'),
    '#type' => 'radios',
    '#required' => TRUE,
    '#default_value' => ';',
    '#options' => array(
      ';' => t('semicolon'),
      ':' => t('colon'),
      ',' => t('comma'),
      'tab' => t('tab'),
    ),
  );
  $form['file_settings']['enclosure'] = array(
    '#title' => t('Text indicator'),
    '#type' => 'radios',
    '#required' => TRUE,
    '#default_value' => '"',
    '#options' => array(
      '"' => t('double qoute'),
      '\'' => t('single quote'),
      'none' => t('none'),
    )
  );
  $form['file_settings']['escape'] = array(
    '#title' => t('Escape character'),
    '#type' => 'textfield',
    '#size' => 1,
    '#maxlength' => 1,
    '#required' => TRUE,
    '#default_value' => '\\'
  );
  // Submit button
  $form['submit_upload'] = array(
    '#type' => 'submit',
    '#value' => 'Import'
  );

  return $form;
}

function svid_leden_import_form_submit($form, &$form_state) {
  error_reporting(E_ALL);
  setlocale(LC_ALL, array('nl_NL.UTF-8', 'en_US.UTF-8'));

  $delimiter = ($form_state['values']['delimiter'] == 'tab') ? "\t" : $form_state['values']['delimiter'];
  $enclosure = ($form_state['values']['enclosure'] == 'none') ? "" : $form_state['values']['enclosure'];
  // Only available as of PHP 5.3.0 in fgetcsv, ignore for now.
  $escape = $form_state['values']['escape'];

  // The following information is mandatory for inclusion in the database
  // Name, e-mail.
  $required_fields = array(
    'personvoornaam',
    'personachternaam',
    'studienummer',
  );

  $file = file_save_upload('import_file');
  if ($file === 0) {
    // No need to set an error, file_save_upload() handles that for us.
    drupal_goto('admin/user/user/import');
    return;
  }

  $fh = fopen($file->filepath, 'r');
  if ($fh === FALSE) {
    drupal_set_message(t('Could not open %file.', array('%file' => $file->filename)), 'error');
    drupal_goto('admin/user/user/import');
    return;
  }

  // We all love statistics!
  $processed = 0;
  $skipped = 0;
  $added = 0;
  $failed = 0;
  
  $firstLine = TRUE;

  while (($line = fgetcsv($fh, 0, $delimiter, $enclosure)) !== FALSE) {
    // Skip empty lines
    if (count($line) === 1 && $line[0] === NULL) {
      continue;
    }

    if ($firstLine === TRUE) {
      $header = $line;
      $fields = count($header);
      
      $firstLine = FALSE;
      continue;
    }

    $data = array();
    $skip = FALSE;

    // Map fields to their respective names to ensure easy access with random columns ordering
    for ($i = 0; $i < $fields; $i++) {
      $data[strtolower($header[$i])] = $line[$i];
    }

    switch (strtolower($data['status'])) {
      case 'ttp-abonnee':
      case 'afgestudeerd':
      case 'geen lid':
        $skipped++;
        $processed++;
        $skip = TRUE;
        break;
      default:
        $skip = FALSE;
        break;
    }

    if ($skip) {
      continue;
    }

    // Build fields that can be entered for a user account
    $member = array ();

    // Unique information
    if (!empty($data['studienummer'])) {
      $member['studentnumber'] = intval($data['studienummer']);
    }
    
    if (empty($data['homeemail'])) {
      $member['mail'] = 'nomail-svid@tudelft.nl';
    }
    else {
      $member['mail'] = $data['homeemail'];
    }

    // Personal information
    $member['firstname'] = $data['personvoornaam'];
    $member['lastname'] = $data['personachternaam'];
    if (!empty($data['persontussenvoegsels'])) {
      $member['prefix'] = $data['persontussenvoegsels'];
      $member['name'] = sprintf('%s %s %s', $member['firstname'], $member['prefix'], $member['lastname']);
    }
    else {
      $member['prefix'] = NULL;
      $member['name'] = sprintf('%s %s', $member['firstname'], $member['lastname']);
    }

    // Check availability for required fields
    foreach ($required_fields as $field) {
      if (empty($data[$field])) {
        drupal_set_message(t('Skipping record: %field is empty. Lidnummer: %lid.', array('%lid' => $data['lidnummerid'], '%field' => $field)), 'warning');
        $skipped++;
        $processed++;
        $skip = TRUE;
        break;
      }
    }

    if ($skip) {
      continue;
    }

    switch (strtolower($data['persongeslacht'])) {
      case 'man':
        $member['gender'] = GENDER_MALE;
        break;
      case 'vrouw':
        $member['gender'] = GENDER_FEMALE;
        break;
      default:
        $member['gender'] = GENDER_UNKNOWN;
        break;
    }
    if (preg_match('#^([0-9]{1,2})-([0-9]{1,2})-([0-9]{4})$#', $data['persongeboortedatum'], $matches)) {
      $member['birthdate'] = $matches[3] * 10000 + $matches[2] * 100 + $matches[1]; // YYYYMMDD
    }
    elseif (preg_match('#^([0-9]{1,2})([0-9]{2})([0-9]{4})$#', $data['persongeboortedatum'], $matches)) {
      $member['birthdate'] = $matches[3] * 10000 + $matches[2] * 100 + $matches[1]; // YYYYMMDD
    }
    elseif (preg_match('#^([0-9]{1,2})([0-9]{2})([0-9]{2})$#', $data['persongeboortedatum'], $matches)) {
      $member['birthdate'] = ($matches[3] + 1900) * 10000 + $matches[2] * 100 + $matches[1]; // YYYYMMDD
    }

    // Residential information
    if (!empty($data['straatnaam']) && !empty($data['nr'])) {
      $member['address'] = $data['straatnaam'] . ' ' . $data['nr'];
    }
    if (!empty($data['homepostcode'])) {
      $member['zipcode'] = str_replace(' ', '', $data['homepostcode']);
    }
    if (!empty($data['homeplaats'])) {
      $member['city'] = $data['homeplaats'];
    }
    if (!empty($data['homeland'])) {
      $member['country'] = strtolower($data['homeland']);
    }

    // Contact information
    if (preg_match('#0([0-9]+)-([0-9]+)#', $data['hometelefoon1'], $matches)) {
      $member['tel_home'] = '+31' . $matches[1] . $matches[2];
    }
    if (preg_match('#0([0-9]+)-([0-9]+)#', $data['hometelefoonmobiel'], $matches)) {
      $member['tel_mobile'] = '+31' . $matches[1] . $matches[2];
    }
    if (preg_match('#0([0-9]+)-([0-9]+)#', $data['hometelefoonouders'], $matches)) {
      $member['tel_parents'] = '+31' . $matches[1] . $matches[2];
    }

    if (!empty($data['studie1ejaar'])) {
      $member['member_since'] = $data['studie1ejaar'];
      $member['student_since'] = $data['studie1ejaar'];
    }

    switch(strtolower($data['status'])) {
      case 'geen lid':
        $member['membership'] = 0;
        break;
      case 'plus lid':
      case 'gewoon lid':
        $member['membership'] = 1;
        break;
      case 'huidig bestuur':
        $member['membership'] = 2;
        break;
      case 'oud-bestuur':
        $member['membership'] = 3;
        break;
      case 'buitengewoon lid':
        $member['membership'] = 5;
        break;
      case 'erelid':
        $member['membership'] = 6;
        break;
      default:
        $member['membership'] = -1;
        break;
    }

    // Membership administration only has student numbers,
    // do not use NetID@tudelft.net (aselectuid).
    // Also, check for studentnumber above 1000000, numbers below are
    // no studentnumbers and will not work with ASelect login.
    if ($member['studentnumber'] >= 1000000) {
      $member['auth_aselect_tudStudentNumber'] = isset($member['studentnumber']) ? $member['studentnumber'] : FALSE;
    }
    
    $member['pass'] = user_password();
    $member['status'] = 1;
    $member['timezone'] = 3600;
    if (!empty ($data['studienummer'])) {
      $member['picture'] = sprintf('sites/default/files/smoelenboek/%s.jpg', $data['studienummer']);
    }

    $result = db_query("SELECT uid FROM {svid_leden} WHERE studentnumber = %d", $data['studienummer']);
    $account = db_fetch_object($result);

    // Save information
    if ($account === FALSE) {
      $account = user_save(NULL, $member);
    }
    else {
      $account = user_save($account, $member);
    }
    
    if (!is_object($account)) {
      drupal_set_message(t('Failed to save user information for %fn', array('%fn' => $member['name'])), 'error');
      $failed++;
      break;
    }
    else {
      $added++;
    }

    $processed++;
  }

  // If $firstLine still is TRUE, no processing has happened.
  // This a good indication that the file is malformed.
  if ($firstLine === TRUE) {
    drupal_set_message(t('%file is not a valid CSV-file. Check the file and the file settings options and try again.', array ('%file' => $file->filename)), 'error');
    drupal_goto('admin/user/user/import');
    return;
  }

  drupal_set_message(t('Processed %proc records: <ul><li>%new new records.</li><li>%skip skipped records.</li><li>%fail failed records.</ul>', array('%proc' => $processed, '%new' => $added, '%skip' => $skipped, '%fail' => $failed)));
  drupal_goto('admin/user/user');
}
