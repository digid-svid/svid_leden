<?php
/**
 * @file svid_leden.admin.inc
 *   Administrative functions. For now, importing a CSV file into the database
 *
 * @author Raymond Jelierse
 */

/**
 * This form is used to submit a dataset to import into the user database.
 *
 * @return
 *   The import form
 */
function svid_leden_import_form() {
  return array(
    '#attributes' => array(
      'enctype' => 'multipart/form-data',
    ),
    'import_file' => array(
      '#title' => t('Source'),
      '#type' => 'file',
    ),
    'file_settings' => array(
      '#title' => t('File settings'),
      '#type' => 'fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      'delimiter' => array(
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
      ),
      'enclosure' => array(
        '#title' => t('Text indicator'),
        '#type' => 'radios',
        '#required' => TRUE,
        '#default_value' => '"',
        '#options' => array(
          '"' => t('double qoute'),
          '\'' => t('single quote'),
          'none' => t('none'),
        ),
      ),
      'escape' => array(
        '#title' => t('Escape character'),
        '#type' => 'textfield',
        '#size' => 1,
        '#maxlength' => 1,
        '#required' => TRUE,
        '#default_value' => '\\',
      ),
    ),
    'upload' => array(
      '#type' => 'submit',
      '#value' => 'Import',
    ),
  );
}

/**
 * Implements hook_submit()
 */
function svid_leden_import_form_submit($form, &$form_state) {
  // Save the uploaded file
  $file = file_save_upload('import_file');
  if ($file === 0) {
    drupal_goto('admin/user/user/import');
  }

  // Import settings to pass to the batch process
  $import = array();
  $import['file'] = $file;
  $import['delimiter'] = ($form_state['values']['delimiter'] == 'tab') ? "\t" : $form_state['values']['delimiter'];
  $import['enclosure'] = ($form_state['values']['enclosure'] == 'none') ? "" : $form_state['values']['enclosure'];
  $import['required_fields'] = array(
    'personvoornaam',
    'personachternaam',
    'studienummer',
  );

  // Create the batch process
  $batch = array(
    'title' => t('Importing members'),
    'operations' => array(
      array('svid_leden_import_batch', array($import)),
    ),
    'finished' => 'svid_leden_import_finished',
    'file' => drupal_get_path('module', 'svid_leden') . '/svid_leden.admin.inc',
  );

  // Attach the batch to the submit handling
  batch_set($batch);
}

/**
 * Open the file to import and extract the individual records.
 *
 * @param $file
 *   The file object from the Drupal file upload process.
 * @return
 *   An array containing the records in the file, of FALSE upon error.
 */
function svid_leden_import_read_file($file) {
  // Get the contents of the file
  $data = file_get_contents($file->filepath);
  if (empty($data)) {
    drupal_set_message(t('Could not open %file.', array('%file' => $file->filename)), 'erorr');
    return FALSE;
  }

  // Explode the contents to single records.
  // str_getcsv() ensures that escaped newlines will be honored.
  $records = str_getcsv($data, "\n", $import['enclosure']);
  if (empty($records)) {
    drupal_set_message(t('File format is not recognized'), 'error');
    return FALSE;
  }

  return $records;
}

/**
 * Batch operation for importing members into the Drupal system.
 *
 * @param $import
 *   Import settings
 * @param $context
 *   The batch context
 */
function svid_leden_import_batch($import, &$context) {
  // Initialize batch process operation
  if (empty($context['sandbox']['header'])) {
    $context['sandbox'] = $import;
    $context['sandbox']['records'] = svid_leden_import_read_file($import['file']);
    $context['sandbox']['processed'] = 0;
    $context['sandbox']['total'] = count($context['sandbox']['records']);
    // Initialize results storage
    $context['results']['skipped'] = array();
    $context['results']['failed'] = array();
    $context['results']['updated'] = array();
    $context['results']['imported'] = array();
    // First line is header
    $line = array_shift($context['sandbox']['records']);
    $context['sandbox']['header'] = str_getcsv($line, $context['sandbox']['delimiter'], $context['sandbox']['enclosure']);
  }

  $header = $context['sandbox']['header'];
  $header_length = count($header);

  // Read a record from the file
  $line = array_shift($context['sandbox']['records']);

  // No more entries in array
  if ($line === NULL) {
    $context['finished'] = 1;
    return;
  }
  $record = str_getcsv($line, $context['sandbox']['delimiter'], $context['sandbox']['enclosure']);

  // Map fields from the record to the header label
  for ($i = 0; $i < $header_length; $i++) {
    $data[strtolower($header[$i])] = trim($record[$i]);
  }

  // Check if we can skip this recored
  if (_can_skip_record($data)) {
    $context['sandbox']['processed']++;
    $context['results']['skipped'][] = $data['lidnummerid'];
    $context['finished'] = $context['sandbox']['processed'] / $context['sandbox']['total'];
    return;
  }

  // Check if the record contains the required fields
  if (_record_has_required_fields($data) === FALSE) {
    $context['sandbox']['processed']++;
    $context['results']['failed'][] = $data['lidnummerid'];
    $context['finished'] = $context['sandbox']['processed'] / $context['sandbox']['total'];
    return;
  }

  $array = array();
  // Unique identifier
  $array['studentnumber'] = intval($data['studienummer']);
  $array['auth_aselect'] = $array['studentnumber'];

  // Basic Drupal information
  $array['name'] = _record_get_name($data);
  $array['mail'] = empty($data['homeemail']) ? $array['studentnumber'] . '@studienummer.tudelft.nl' : $data['homeemail'];

  // Personalia
  $array['firstname'] = $data['personvoornaam'];
  $array['lastname'] = $data['personachternaam'];
  $array['prefix'] = !empty($data['persontussenvoegsels']) ? $data['persontussenvoegsels'] : NULL;
  $array['gender'] = _record_get_gender($data['persongeslacht']);
  $array['birthdate'] = _record_get_birthdate($data['persongeboortedatum']);

  // Residence
  $array['address'] = (empty($data['straatnaam']) && empty($data['nr'])) ? NULL : trim($data['straatnaam'] . ' ' . $data['nr']);
  $array['zipcode'] = !empty($data['homepostcode']) ? str_replace(' ', '', $data['homepostcode']) : NULL;
  $array['city'] = !empty($data['homeplaats']) ? $data['homeplaats'] : NULL;
  $array['country'] = !empty($data['homeland']) ? strtolower($data['homeland']) : NULL;

  // Contact
  $array['tel_home'] = _record_get_telephone_number(!empty($data['hometelefoon1']) ? $data['hometelefoon1'] : $data['hometelefoon2']);
  $array['tel_mobile'] = _record_get_telephone_number($data['hometelefoonmobiel']);
  $array['tel_parents'] = _record_get_telephone_number($data['hometelefoonouders']);

  // Membership information
  $array['membership'] = _record_get_membership_status($data['status']);
  $array['member_since'] = $data['studie1ejaar'];
  $array['student_since'] = $data['studie1ejaar'];

  // Check for existing account
  $result = db_query('SELECT uid FROM {svid_leden} WHERE studentnumber = %d', $array['studentnumber']);
  if ($result !== FALSE) {
    $uid = db_result($result);
  }

  // New account
  if ($uid === FALSE) {
    $account = NULL;
    $array['pass'] = user_password();
    $array['status'] = 1;
    $array['timezone'] = 3600;
  }
  else {
    $update = TRUE;
    $account->uid = $uid;
  }

  // Save account information
  $account = user_save($account, $array);

  // Check status of save
  if ($account === FALSE) {
    $context['results']['failed'][] = $data['lidnummerid'];
  }
  elseif ($update) {
    $context['results']['updated'][] = $account->uid;
  }
  else {
    $context['results']['imported'][] = $account->uid;
  }

  // Update counter
  $context['sandbox']['processed']++;
  $context['finished'] = $context['sandbox']['processed'] / $context['sandbox']['total'];
}

function svid_leden_import_finished($success, $results, $operations) {
  // Get individual counts
  $skipped = count($results['skipped']);
  $failed = count($results['failed']);
  $updated = count($results['updated']);
  $imported = count($results['imported']);

  // Get processed by summing individuals
  $processed = $skipped + $failed + $updated + $imported;

  // Print status messages
  if ($success) {
    drupal_set_message(t('Finished importing members.'));
  }
  else {
    drupal_set_message(t('An error occured in the batch process'), 'error');
  }

  drupal_set_message(format_plural($processed, 'Processed 1 record', 'Processed @count records'));
  drupal_set_message(format_plural($imported, 'Imported 1 user', 'Imported @count users'));
  drupal_set_message(format_plural($updated, 'Updated 1 user', 'Updated @count users'));
  drupal_set_message(format_plural($skipped, '1 record skipped', '@count records skipped'));
  drupal_set_message(format_plural($failed, '1 record failed', '@count records failed'));
  foreach ($results['failed'] as $lidnummer) {
    drupal_set_message(t('Importing record %id failed', array('%id' => $lidnummer)), 'error');
  }

  drupal_goto('admin/user/user');
}

/**
 * Check if a record can be skipped
 *
 * @param $record
 *   The record to check
 * @return
 *   TRUE if the record can be skipped, FALSE if it contains valid data
 */
function _can_skip_record($record) {
  if (intval($record['studienummer']) < 1000000) {
    return TRUE;
  }

  switch ($record['status']) {
    case 'ttp-abonnee':
    case 'afgestudeerd':
    case 'geen lid':
      return TRUE;
  }

  return FALSE;
}

function _record_has_required_fields($record) {
  return !(empty($record['personvoornaam']) || empty($record['personachternaam']));
}

function _record_get_name($record) {
  if (empty($record['persontussenvoegsels'])) {
    return sprintf('%s %s', $record['personvoornaam'], $record['personachternaam']);
  }
  else {
    return sprintf('%s %s %s', $record['personvoornaam'], $record['persontussenvoegsels'], $record['personachternaam']);
  }
}

function _record_get_gender($gender) {
  switch (strtolower($gender)) {
    case 'man':
    case 'm':
      return GENDER_MALE;
    case 'vrouw':
    case 'v':
      return GENDER_FEMALE;
    default:
      return GENDER_UNKNOWN;
  }
}

function _record_get_birthdate($birthdate) {
  if (preg_match('#^([0-9]{1,2})-([0-9]{1,2})-([0-9]{4})$#', $birthdate, $matches)) {
    return $matches[3] * 10000 + $matches[2] * 100 + $matches[1]; // YYYYMMDD
  }
  elseif (preg_match('#^([0-9]{1,2})([0-9]{2})([0-9]{4})$#', $birthdate, $matches)) {
    return $matches[3] * 10000 + $matches[2] * 100 + $matches[1]; // YYYYMMDD
  }
  elseif (preg_match('#^([0-9]{1,2})([0-9]{2})([0-9]{2})$#', $birthdate, $matches)) {
    return ($matches[3] + 1900) * 10000 + $matches[2] * 100 + $matches[1]; // YYYYMMDD
  }

  return NULL;
}

function _record_get_telephone_number($number) {
  $number = str_replace(array(' ', '-'), '', $number);
  // Test for internation number
  if (preg_match('#^00([0-9]+)$#', $number, $matches)) {
    return '+' . $matches[1];
  }
  // Test for national number
  elseif (preg_match('#^0([0-9]+)$#', $number, $matches)) {
    return '+31' . $matches[1];
  }

  return NULL;
}

function _record_get_membership_status($status) {
  switch (strtolower($status)) {
    case 'geen lid':
      return 0;
    case 'plus lid':
    case 'gewoon lid':
      return 1;
    case 'huidig bestuur':
      return 2;
    case 'oud-bestuur':
      return 3;
    case 'buitengewoon lid':
      return 4;
    case 'erelid':
      return 5;
    default:
      return -1;
  }
}

if (!function_exists('str_getcsv')) {
  function str_getcsv($input, $delimiter=',', $enclosure='"', $escape=null, $eol=null) {
    $temp=fopen("php://memory", "rw");
    fwrite($temp, $input);
    fseek($temp, 0);
    $r = array();
    while (($data = fgetcsv($temp, 4096, $delimiter, $enclosure)) !== false) {
      $r[] = $data;
    }
    fclose($temp);
    return $r;
  }
}