<?php
/**
 * @file
 * Administrative forms section.
 */

/**
 * Administration settings form.
 */
function seven_admin_form($form, &$form_state) {
    $config = config('seven.settings');

    $form['seven_api_key'] = [
        '#default_value' => $config->get('seven_api_key'),
        '#description' => t('Create one @ https://app.seven.io/developer'),
        '#required' => true,
        '#title' => t('API Key'),
        '#type' => 'textfield',
    ];

    $from = $config->get('seven_from');
    if (!$from) {
        $siteName = config('system.core')->get('site_name');
        $length = strlen($siteName);

        if (is_numeric($siteName)) {
            if ($length <= 16) $from = $siteName;
        } elseif ($length <= 11) $from = $siteName;
    }
    $form['seven_from'] = [
        '#default_value' => $from,
        '#description' => t('Optionally set a custom sender identifier'),
        '#maxlength' => 16,
        '#title' => t('From'),
        '#type' => 'textfield',
    ];

    $form['seven_registration_form'] = [
        '#default_value' => $config->get('seven_registration_form'),
        '#description' => t('If not disabled, render a text input which can be required.'),
        '#options' => [t('Disabled'), t('Optional'), t('Required')],
        '#title' => t('Collect phone number during registration?'),
        '#type' => 'radios',
    ];

    $form['submit'] = [
        '#type' => 'submit',
        '#value' => t('Save seven Settings'),
    ];

    return $form;
}

/**
 * Backdrop submit for config, replace call to system_settings_form()
 */
function seven_admin_form_submit($form, &$form_state) {
    $keys = [
        'seven_from',
        'seven_api_key',
        'seven_registration_form',
    ];
    $config = config('seven.settings');

    foreach ($keys as $k) $config->set($k, $form_state['values'][$k]);

    $config->save();

    backdrop_set_message('Configuration saved for seven');
}

/**
 * Administrative form for testing messages.
 */
function seven_admin_test_form($form, &$form_state) {
    $args = $form_state['build_info']['args'];
    $phone = isset($args[1]->seven_user->phone) ? $args[1]->seven_user->phone : null;

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

    if ($phone) $form['number']['#value'] = $phone;

    $form['submit'] = [
        '#type' => 'submit',
        '#value' => t('Send Message'),
    ];

    return $form;
}

/**
 * Validation handler for the administrative test message form.
 */
function seven_admin_test_form_validate($form, &$form_state) {
    $values = $form_state['values'];
    $type = $values['message_type'];
    $maxLength = 'sms' === $type ? 1520 : 10000;

    if (strlen($values['message']) > $maxLength)
        form_set_error('message',
            t('Message of type @type may not exceed @maxLength characters.', [
                '@maxLength' => $maxLength,
                '@type' => $type,
            ]));

    if (!is_numeric($values['number']))
        form_set_error('number', t('You must enter a valid phone number.'));
}

/**
 * Submit handler for the administrative test message testing form.
 */
function seven_admin_test_form_submit($form, &$form_state) {
    $values = $form_state['values'];
    $fn = 'seven_send_' . $values['message_type'];

    if ($fn($values['number'], $values['message'])) {
        $msg = 'Your message has been sent';
        $type = 'status';
    } else {
        $msg = 'Unable to send your message.';
        $type = 'error';
    }

    backdrop_set_message(t($msg), $type);
}
