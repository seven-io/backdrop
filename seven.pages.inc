<?php
/**
 * @file
 * User facing pages.
 */

/**
 * Menu callback for adding and confirming a user's phone number.
 */
function seven_user_settings($account) {
    return backdrop_get_form('seven_user_settings_form', $account);
}

/**
 * User settings form.
 */
function seven_user_settings_form($form, &$form_state, $account) {
    if (isset($form_state['values']['account']))
        $account = user_load($form_state['values']['account']->uid);

    $form['#prefix'] = '<div id=\'seven-user-form\'>';
    $form['#suffix'] = '</div>';
    $form['uid'] = [
        '#type' => 'hidden',
        '#value' => $account->uid,
    ];

    $seven = isset($account->seven_user) ? $account->seven_user : null;

    if (!$seven) {
        $form['number'] = [
            '#description' =>
                t('A confirmation code will be sent to the number provided.'),
            '#maxlength' => 16,
            '#required' => true,
            '#size' => 16,
            '#title' => t('Phone number'),
            '#type' => 'textfield',
        ];

        $form['submit'] = [
            '#ajax' => [
                'callback' => 'seven_user_settings_ajax_callback',
                'method' => 'replace',
                'wrapper' => 'seven-user-form',
            ],
            '#submit' => ['seven_user_settings_add_form_submit'],
            '#type' => 'submit',
            '#validate' => ['seven_user_settings_add_form_validate'],
            '#value' => t('Confirm number'),
        ];
    } elseif ((int)$seven->status === SEVEN_USER_STATUS_PENDING) {
        $form['number'] = [
            '#markup' => $seven->phone,
            '#title' => t('Phone number'),
            '#type' => 'item',
        ];

        $form['confirm_code'] = [
            '#description' => t('Enter the confirmation code sent to your phone.'),
            '#maxlength' => 5,
            '#size' => 5,
            '#title' => t('Confirmation code'),
            '#type' => 'textfield',
        ];

        $form['submit'] = [
            '#ajax' => [
                'callback' => 'seven_user_settings_ajax_callback',
                'method' => 'replace',
                'wrapper' => 'seven-user-form',
            ],
            '#submit' => ['seven_user_settings_confirm_form_submit'],
            '#type' => 'submit',
            '#validate' => ['seven_user_settings_confirm_form_validate'],
            '#value' => t('Confirm number'),
        ];

        $form['reset'] = [
            '#ajax' => [
                'callback' => 'seven_user_settings_ajax_callback',
                'method' => 'replace',
                'wrapper' => 'seven-user-form',
            ],
            '#submit' => ['seven_user_settings_confirm_form_submit'],
            '#type' => 'submit',
            '#validate' => ['seven_user_settings_confirm_form_validate'],
            '#value' => t('Delete & start over'),
        ];
    } elseif ((int)$seven->status === SEVEN_USER_STATUS_CONFIRMED) {
        $form['seven_user']['number'] = [
            '#description' => t('Your phone number has been confirmed.'),
            '#markup' => $seven->phone,
            '#title' => t('Your phone number'),
            '#type' => 'item',
        ];

        $form['reset'] = [
            '#ajax' => [
                'callback' => 'seven_user_settings_ajax_callback',
                'method' => 'replace',
                'wrapper' => 'seven-user-form',
            ],
            '#submit' => ['seven_user_settings_reset_form_submit'],
            '#type' => 'submit',
            '#validate' => ['seven_user_settings_reset_form_validate'],
            '#value' => t('Delete & start over'),
        ];

        $form['reset_help'] = [
            '#type' => 'help',
            '#markup' => t('Delete number and validate a new one.'),
            # '#weight' => -5,
        ];
    }

    return $form;
}

/**
 * Ajax callback for user forms.
 */
function seven_user_settings_ajax_callback($form, &$form_state) {
    return $form;
}

/**
 * Validation function for user settings form.
 */
function seven_user_settings_add_form_validate($form, &$form_state) {
    $number = $form_state['values']['number'];

    if (!is_numeric($number))
        $err = 'You must enter a valid phone number.';
    elseif (seven_verify_duplicate_number($number))
        $err = 'Number already in use and can not be assigned to more than one account.';

    if (isset($err)) form_set_error('number', t($err));
}

/**
 * Form for composing message to user.
 */
function seven_compose_user_message($form, &$form_state) {
    $user = $form_state['build_info']['args'][1];

    $form['message_type'] = [
        '#default_value' => 'sms',
        '#options' => ['sms' => t('SMS'), 'voice' => t('Text-To-Speech')],
        '#required' => true,
        '#title' => t('Message Type'),
        '#type' => 'radios',
    ];

    $form['message'] = [
        '#description' => t('The text of your message.'),
        '#maxlength' => 10000,
        '#required' => true,
        '#title' => t('Message'),
        '#type' => 'textarea',
    ];

    $form['number'] = [
        '#description' => t('The number to send your message to.'),
        '#maxlength' => 16,
        '#required' => true,
        '#title' => t('Phone Number'),
        '#type' => 'textfield',
    ];

    $form['submit'] = [
        '#type' => 'submit',
        '#value' => t('Send Message'),
    ];

    return $form;
}

/**
 * Submit handler for user settings form.
 */
function seven_user_settings_add_form_submit($form, &$form_state, $account = null) {
    $values = $form_state['values'];
    if (!$account) $account = user_load($values['uid']);
    $account = seven_user_send_confirmation($account, $values['number']);
    $form_state['values']['account'] = $account;
    $form_state['values']['rebuild'] = true;

    backdrop_set_message(t('A confirmation code has been sent to your phone.'));
}

/**
 * Validation handler for user settings confirmation form.
 */
function seven_user_settings_confirm_form_validate($form, &$form_state) {
    if ($form_state['clicked_button']['#value'] != t('Confirm number')) return;

    $values = $form_state['values'];
    $code = user_load($values['uid'])->seven_user->code;

    if ($values['confirm_code'] !== $code)
        form_set_error('confirm_code', t('The confirmation code is invalid.'));
}

/**
 * Submit handler for user settings confirmation form.
 */
function seven_user_settings_confirm_form_submit($form, &$form_state) {
    $values = $form_state['values'];
    $uid = $values['uid'];
    $account = user_load($uid);

    if ($form_state['clicked_button']['#value'] == t('Delete & start over')) {
        seven_user_delete($account);
        $account = user_load($uid, true);
        $msg = 'Your mobile information has been removed.';
    } else {
        $account->seven_user->status = SEVEN_USER_STATUS_CONFIRMED;
        seven_user_save($account);
        $msg = 'Your mobile number has been confirmed.';
    }

    backdrop_set_message(t($msg));

    $form_state['values']['account'] = $account;
    $form_state['rebuild'] = true;
}

/**
 * Validation handler for user settings reset form.
 */
function seven_user_settings_reset_form_validate($form, &$form_state) {
}

/**
 * Submit handler for user settings reset form.
 */
function seven_user_settings_reset_form_submit($form, &$form_state) {
    $uid = $form_state['values']['uid'];
    seven_user_delete(user_load($uid));
    backdrop_set_message(t('Your phone information has been removed.'));
    $form_state['values']['account'] = user_load($uid, true);
    $form_state['rebuild'] = true;
}
