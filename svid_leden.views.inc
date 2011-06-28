<?php
/**
 * svid_smoelenboek module: views.
 *
 * Copyright 2010 Studievereniging i.d
 * Author: Raymond Jelierse
 *
 * Provided as-is.
 */

/**
 * Implementation of hook_views_data()
 */
function svid_leden_views_data() {
  $data = array(
    'svid_leden' => array(
      'table' => array(
        'group' => t('i.d Facebook'),
        'base' => array(
          'field' => 'uid',
          'title' => t('i.d Data'),
          'help' => t('Extended user information for use in the facebook'),
          'weight' => -10,
        ),
        'join' => array(
          'users' => array(
            'left_field' => 'uid',
            'field' => 'uid',
          ),
        ),
      ),
    ),
  );
	
	$data['svid_leden']['firstname'] = array(
		'title' => t('Firstname'), // The item it appears as on the UI,
		'help' => t('Firstname of the user'),
		'field' => array(
			'handler' => 'views_handler_field',
		),
		'sort' => array(
			'handler' => 'views_handler_sort',
		),
		'filter' => array(
			'handler' => 'views_handler_filter_string',
		),
		'argument' => array(
			'handler' => 'views_handler_argument_string',
		),
	);
	
	$data['svid_leden']['lastname'] = array(
		'title' => t('Lastname'), // The item it appears as on the UI,
		'help' => t('Lastname of the user'),
		'field' => array(
			'handler' => 'views_handler_field',
		),
		'sort' => array(
			'handler' => 'views_handler_sort',
		),
		'filter' => array(
			'handler' => 'views_handler_filter_string',
		),
		'argument' => array(
			'handler' => 'views_handler_argument_string',
		),
	);
	
	$data['svid_leden']['member_since'] = array(
		'title' => t('Member since'), // The item it appears as on the UI,
		'help' => t('First year of membership'),  
		'field' => array(
			'handler' => 'views_handler_field_numeric',
		),
		'filter' => array(
			'handler' => 'views_handler_filter_numeric',
			'numeric' => TRUE,
		),
		'argument' => array(
			'handler' => 'views_handler_argument_numeric',
			'numeric' => TRUE,
		),
	);
	
	$data['svid_leden']['student_since'] = array(
		'title' => t('Student since'), // The item it appears as on the UI,
		'help' => t('First year of enrollment'),  
		'field' => array(
			'handler' => 'views_handler_field_numeric',
		),
		'filter' => array(
			'handler' => 'views_handler_filter_numeric',
			'numeric' => TRUE,
		),
		'argument' => array(
			'handler' => 'views_handler_argument_numeric',
			'numeric' => TRUE,
		),
	);

	$data['svid_leden']['studentnumber'] = array(
		'title' => t('Student number'), // The item it appears as on the UI,
		'help' => t('The student number of the user'),
		'field' => array(
			'handler' => 'views_handler_field_numeric',
		),
		'filter' => array(
			'handler' => 'views_handler_filter_numeric',
			'numeric' => TRUE,
		),
		'argument' => array(
			'handler' => 'views_handler_argument_numeric',
			'numeric' => TRUE,
		),
	);

	$data['svid_leden']['status'] = array(
		'title' => t('Membership status'),
		'help' => t('The current status of the member'),
		'field' => array(
			'handler' => 'views_handler_field_numeric',
		),
		'filter' => array(
			'handler' => 'views_handler_filter_numeric',
			'numeric' => TRUE,
		),
		'argument' => array(
			'handler' => 'views_handler_argument_numeric',
			'numeric' => TRUE,
		),
	);

	return $data;
}