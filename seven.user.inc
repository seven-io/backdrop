<?php
/**
 * @file
 * seven user account and registration related functionality.
 */

/**
 * Implements hook_user_load().
 */
function seven_user_load($users) {
    $rows = db_select('seven_user', 'u')
        ->fields('u')->condition('uid', array_keys($users), 'IN')
        ->execute()->fetchAll();

    foreach ($rows as $row) {
        $uid = $row->uid;
        if (!empty($uid)) $users[$uid]->seven_user = $row;
    }
}

/**
 * Implements hook_user_insert().
 */
function seven_user_insert($account) {
    seven_user_save($account);
}

/**
 * Implements hook_user_update().
 */
function seven_user_update($account) {
    seven_user_save($account);
}

/**
 * Implements hook_user_delete().
 */
function seven_user_delete($account) {
    db_delete('seven_user')->condition('uid', $account->uid)->execute();
}

/**
 * Saves phone data to the {seven_user} table in the database.
 */
function seven_user_save($account) {
    if (!isset($account->seven_user->status)) return;

    $isConfirmed = (int)$account->seven_user->status === SEVEN_USER_STATUS_CONFIRMED;
    $primaryKeys = [];
    if ($isConfirmed) $primaryKeys[] = 'uid';

    backdrop_write_record('seven_user', $account->seven_user, $primaryKeys);
}

/**
 * Implements hook_user_login().
 */
function seven_user_login(&$edit, $account) {
    $hasAccess = seven_edit_access($account);
    $lacksSeven = empty($account->seven_user);
    $isConfirmed = (int)$account->seven_user->status !== SEVEN_USER_STATUS_PENDING;

    if (!$hasAccess || $lacksSeven || $isConfirmed) return;

    $link = l(t('account settings page'), 'user/' . $account->uid . '/edit/seven');

    backdrop_set_message(t('Please confirm your phone number by entering the verification code sent to you.
     Visit !link to enter your verification code.', ['!link' => $link]), 'warning');
}

/**
 * Implements hook_FORMID_form_alter().
 */
function seven_form_user_register_form_alter(&$form, &$form_state, $form_id) {
    $registrationForm = config_get('seven.settings', 'seven_registration_form');

    if (!$registrationForm) return;

    $form['account']['number'] = [
        '#required' => $registrationForm == 2,
        '#title' => t('Phone number'),
        '#type' => 'textfield',
    ];
    $form['#validate'][] = 'seven_register_validate';
    $form['#submit'][] = 'seven_register_submit';
}

/**
 * Custom validation function for phone numbers during registration.
 */
function seven_register_validate($form, &$form_state) {
    $number = $form_state['values']['number'];

    if (empty($number) && empty($form['account']['number']['#required'])) return;

    if (!is_numeric($number)) $err = 'You must enter a valid phone number.';
    elseif (seven_verify_number($number))
        $err = 'Number already in use and can not be assigned to more than one account.';

    if (isset($err)) form_set_error('number', t($err));
}

/**
 * Custom submit handler for phone numbers during registration.
 */
function seven_register_submit($form, &$form_state) {
    $values = $form_state['values'];
    $number = $values['number'];

    if (empty($number) && empty($form['account']['number']['#required'])) return;

    seven_user_send_confirmation(user_load($values['uid']), $number);
}

/**
 * Send a confirmation message.
 * @param object $account
 *   The user object of the account to message.
 * @param string $phone
 *   The phone number to send the message to.
 * @return object
 *   The account of the user.
 */
function seven_user_send_confirmation($account, $phone) {
    $code = mt_rand(10000, 99999);
    $phone = preg_replace('/[^0-9]/', '', $phone);
    $account->seven_user = (object)(compact('code', 'phone') + [
            'status' => SEVEN_USER_STATUS_PENDING,
            'uid' => $account->uid,
        ]);

    seven_user_save($account);
    seven_send_sms($phone, t('Confirmation code: @code', ['@code' => $code]));

    return $account;
}
