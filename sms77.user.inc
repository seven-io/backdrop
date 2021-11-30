<?php
/**
 * @file
 * sms77 user account and registration related functionality.
 */

/**
 * Implements hook_user_load().
 */
function sms77_user_load($users) {
    $rows = db_select('sms77_user', 'u')
        ->fields('u')->condition('uid', array_keys($users), 'IN')
        ->execute()->fetchAll();

    foreach ($rows as $row) {
        $uid = $row->uid;
        if (!empty($uid)) $users[$uid]->sms77_user = $row;
    }
}

/**
 * Implements hook_user_insert().
 */
function sms77_user_insert($account) {
    sms77_user_save($account);
}

/**
 * Implements hook_user_update().
 */
function sms77_user_update($account) {
    sms77_user_save($account);
}

/**
 * Implements hook_user_delete().
 */
function sms77_user_delete($account) {
    db_delete('sms77_user')->condition('uid', $account->uid)->execute();
}

/**
 * Saves phone data to the {sms77_user} table in the database.
 */
function sms77_user_save($account) {
    if (!isset($account->sms77_user->status)) return;

    $isConfirmed = (int)$account->sms77_user->status === SMS77_USER_STATUS_CONFIRMED;
    $primaryKeys = [];
    if ($isConfirmed) $primaryKeys[] = 'uid';

    backdrop_write_record('sms77_user', $account->sms77_user, $primaryKeys);
}

/**
 * Implements hook_user_login().
 */
function sms77_user_login(&$edit, $account) {
    $hasAccess = sms77_edit_access($account);
    $lacksSms77 = empty($account->sms77_user);
    $isConfirmed = (int)$account->sms77_user->status !== SMS77_USER_STATUS_PENDING;

    if (!$hasAccess || $lacksSms77 || $isConfirmed) return;

    $link = l(t('account settings page'), 'user/' . $account->uid . '/edit/sms77');

    backdrop_set_message(t('Please confirm your phone number by entering the verification code sent to you.
     Visit !link to enter your verification code.', ['!link' => $link]), 'warning');
}

/**
 * Implements hook_FORMID_form_alter().
 */
function sms77_form_user_register_form_alter(&$form, &$form_state, $form_id) {
    $registrationForm = config_get('sms77.settings', 'sms77_registration_form');

    if (!$registrationForm) return;

    $form['account']['number'] = [
        '#required' => $registrationForm == 2,
        '#title' => t('Phone number'),
        '#type' => 'textfield',
    ];
    $form['#validate'][] = 'sms77_register_validate';
    $form['#submit'][] = 'sms77_register_submit';
}

/**
 * Custom validation function for phone numbers during registration.
 */
function sms77_register_validate($form, &$form_state) {
    $number = $form_state['values']['number'];

    if (empty($number) && empty($form['account']['number']['#required'])) return;

    if (!is_numeric($number)) $err = 'You must enter a valid phone number.';
    elseif (sms77_verify_number($number))
        $err = 'Number already in use and can not be assigned to more than one account.';

    if (isset($err)) form_set_error('number', t($err));
}

/**
 * Custom submit handler for phone numbers during registration.
 */
function sms77_register_submit($form, &$form_state) {
    $values = $form_state['values'];
    $number = $values['number'];

    if (empty($number) && empty($form['account']['number']['#required'])) return;

    sms77_user_send_confirmation(user_load($values['uid']), $number);
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
function sms77_user_send_confirmation($account, $phone) {
    $code = mt_rand(10000, 99999);
    $phone = preg_replace('/[^0-9]/', '', $phone);
    $account->sms77_user = (object)(compact('code', 'phone') + [
            'status' => SMS77_USER_STATUS_PENDING,
            'uid' => $account->uid,
        ]);

    sms77_user_save($account);
    sms77_send_sms($phone, t('Confirmation code: @code', ['@code' => $code]));

    return $account;
}