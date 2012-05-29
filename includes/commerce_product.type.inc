<?php

/**
 * @file
 * Provides functionality for inline managing commerce products.
 */

/**
 * IEF settings callback: Returns a settings form embedded into the IEF widget
 * settings form. The returned settings are later available in form callbacks
 * through $form['#settings'].
 */
function inline_entity_form_commerce_product_settings($settings, $instance) {
  $form = array();
  $form['autogenerate_title'] = array(
    '#type' => 'checkbox',
    '#title' => t('Auto generate the product title'),
    '#description' => t('This will hide the title input field and generate the title by appending any available attributes to the @entity title.', array('@entity' => $instance['entity_type'])),
    '#default_value' => isset($settings['autogenerate_title']) ? $settings['autogenerate_title'] : TRUE,
  );

  return $form;
}

/**
 * IEF default fields callback: Returns an array of fields (which can be either
 * Field API fields or properties provided through hook_field_extra_fields())
 * that should be used to represent a selected product.
 *
 * The IEF widget can have its own fields specified in the widget settings, in
 * which case the output of this function is ingored.
 *
 * @param $types
 *   An array of allowed product types for this widget.
 *
 * @return
 *   An array of field information, keyed by field name. Allowed keys:
 *   - type: 'field' or 'extra_field',
 *   - label: Human readable name of the field, shown to the user.
 *   - weight: The position of the field relative to other fields.
 *   - visible: Whether the field should be displayed.
 *   Special keys for type 'field':
 *   - formatter: The formatter used to display the field, or "hidden".
 *   - settings: An array passed to the formatter. If empty, defaults are used.
 *   - delta: If provided, limits the field to just the specified delta.
 */
function inline_entity_form_commerce_product_default_fields($types) {
  $fields = array();

  $fields['title'] = array(
    'type' => 'extra_field',
    'label' => t('Variation title'),
    'visible' => TRUE,
    'weight' => 1,
  );
  $fields['sku'] = array(
    'type' => 'extra_field',
    'label' => 'SKU',
    'visible' => TRUE,
    'weight' => 2,
  );

  // If only one product type is allowed, its fields can be used as columns.
  if (count($types) == 1) {
    $type = reset($types);

    foreach (field_info_instances('commerce_product', $type) as $field_name => $instance) {
      $field = field_info_field($field_name);

      // If the product has an imagefield, show it.
      if ($field['type'] == 'image') {
        $fields[$field_name] = array(
          'type' => 'field',
          'label' => $instance['label'],
          'formatter' => 'image',
          'settings' => array('image_style' => 'thumbnail'),
          'delta' => 0,
          'visible' => TRUE,
          'weight' => -10,
        );
        // Don't add any other imagefields. One is enough.
        break;
      }
    }

    // If the type has up to 3 attributes, show them instead of the title field.
    $attributes = _inline_entity_form_commerce_product_attributes($type);
    if (count($attributes) <= 3) {
      $fields['title']['visible'] = FALSE;

      foreach ($attributes as $field_name => $attribute) {
        $field_type = field_info_field_types($attribute['field']['type']);

        $weight = -3;
        $fields[$field_name] = array(
          'type' => 'field',
          'label' => $attribute['instance']['label'],
          'formatter' => $field_type['default_formatter'],
          'settings' => array(),
          'visible' => TRUE,
          'weight' => ++$weight,
        );
      }
    }
  }

  $fields['commerce_price'] = array(
    'type' => 'field',
    'label' => t('Price'),
    'formatter' => 'commerce_price_formatted_amount',
    'settings' => array(),
    'visible' => TRUE,
    'weight' => 99,
  );

  return $fields;
}

/**
 * IEF add/edit form callback: Returns the product form to be embedded.
 *
 * When adding data to $form_state it should be noted that there can be several
 * IEF widgets on one master form, each with several form rows, leading to
 * possible key collisions if the keys are not prefixed with $parents.
 */
function inline_entity_form_commerce_product_form($form, &$form_state) {
  $product = $form['#entity'];

  $form['#element_validate'] = array(
    'inline_entity_form_commerce_product_form_validate',
  );
  $form['#element_submit'] = array(
    'inline_entity_form_commerce_product_form_submit',
  );

  $form['product_attributes'] = array(
    '#type' => 'fieldset',
    '#title' => t('Attributes'),
    '#attributes' => array('class' => array('container-inline', 'ief-product-attributes', 'ief-entity-fieldset')),
  );
  $form['product_details'] = array(
    '#type' => 'fieldset',
    '#title' => t('Details'),
    '#attributes' => array('class' => array('ief-product-details', 'ief-entity-fieldset')),
  );
  $form['product_image'] = array(
    '#type' => 'fieldset',
    '#title' => t('Variation image'),
    '#attributes' => array('class' => array('ief-product-image', 'ief-entity-fieldset')),
  );

  $language = !empty($product->language) ? $product->language : LANGUAGE_NONE;
  $form['sku'] = array(
    '#type' => 'textfield',
    '#title' => t('SKU'),
    '#description' => t('Supply a unique identifier using letters, numbers, hyphens, and underscores. Commas may not be used.'),
    '#default_value' => $product->sku,
    '#maxlength' => 128,
    '#required' => TRUE,
    '#fieldset' => 'product_details',
    '#weight' => -10,
  );
  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Variation Title'),
    '#default_value' => $product->title,
    '#maxlength' => 128,
    '#required' => TRUE,
    '#fieldset' => 'product_details',
    '#weight' => -9,
  );
  // Hide or disable the SKU field if it is auto-generated by Commerce AutoSKU.
  if (module_exists('commerce_autosku') && $settings = commerce_autosku_get_settings($product)) {
    $form['sku']['#required'] = FALSE;
    $form['sku']['#disabled'] = TRUE;
    if ($settings['advanced']['hide_sku']) {
      $form['sku']['#access'] = FALSE;
    }
    else {
      $form['sku']['#description'] = t('SKU will be automatically generated.');
    }
  }
  // Hide the title field if it is auto-generated.
  if (!empty($form['#settings']['autogenerate_title'])) {
    $form['title']['#required'] = FALSE;
    $form['title']['#access'] = FALSE;
  }

  // Attach fields.
  field_attach_form('commerce_product', $product, $form, $form_state, $language);

  // Arrange attributes.
  $attributes = _inline_entity_form_commerce_product_attributes($product->type);
  if (empty($attributes)) {
    // Hide the fieldset, it will be empty.
    $form['product_attributes']['#access'] = FALSE;
  }
  else {
    foreach ($attributes as $field_name => $attribute) {
      $form[$field_name]['#fieldset'] = 'product_attributes';
    }
  }

  // Arrange images and other non-attribute fields.
  $imagefields = array();
  foreach (field_info_instances('commerce_product', $product->type) as $name => $instance) {
    $field_name = $instance['field_name'];
    $field = field_info_field($field_name);

    if ($field['type'] == 'image') {
      $form[$field_name]['#fieldset'] = 'product_image';
      $imagefields[] = $field_name;
    }
    elseif (!isset($attributes[$field_name])) {
      $form[$field_name]['#fieldset'] = 'product_details';
    }
  }

  if (count($imagefields) == 0) {
    // The fieldset is empty, hide it.
    $form['product_image']['#access'] = FALSE;
  }
  elseif (count($imagefields) == 1) {
    // There's only one image on the product, no need to show its title, the
    // fieldset title is enough.
    $field_name = reset($imagefields);
    $language = $form[$field_name]['#language'];
    unset($form[$field_name][$language][0]['#title']);
    // Prevent multi-value imagefields from rendering their own fieldsets.
    if (isset($form[$field_name][$language]['#theme_wrappers']) && in_array('fieldset', $form[$field_name][$language]['#theme_wrappers'])) {
      $key = array_search('fieldset', $form[$field_name][$language]['#theme_wrappers']);
      unset($form[$field_name][$language]['#theme_wrappers'][$key]);
    }
  }

  return $form;
}

/**
 * IEF add/edit form validation callback.
 */
function inline_entity_form_commerce_product_form_validate(&$form, &$form_state) {
  $product = &$form['#entity'];

  $parents_path = implode('][', $form['#parents']);
  $product_values = drupal_array_get_nested_value($form_state['values'], $form['#parents']);
  $sku = trim($product_values['sku']);

  // Ensure the proposed SKU is unique.
  if (!commerce_product_validate_sku_unique($sku, $product->product_id)) {
    form_set_error($parents_path . '][sku', t('This SKU is already in use and must be unique. Please supply another value.'));
  }
  // Validate the SKU for invalid characters.
  if (!commerce_product_validate_sku($sku)) {
    form_set_error($parents_path . '][sku', t('The SKU %sku contains invalid characters.', array('%sku' => $sku)));
  }
  // Trim leading and trailing whitespace from the SKU.
  drupal_array_set_nested_value($form_state['values'], array_merge($form['#parents'], array('sku')), $sku);

  field_attach_form_validate('commerce_product', $product, $form, $form_state);
}

/**
 * IEF add/edit form submit callback.
 */
function inline_entity_form_commerce_product_form_submit(&$form, &$form_state) {
  global $user;

  $product_values = drupal_array_get_nested_value($form_state['values'], $form['#parents']);
  $product = &$form['#entity'];
  $product->sku = $product_values['sku'];
  // Assign newly created products to the current user.
  if (empty($product->product_id)) {
    $product->uid = $user->uid;
  }

  field_attach_submit('commerce_product', $product, $form, $form_state);
  inline_entity_form_cleanup_field_form_state($form, $form_state);

  if (empty($form['#settings']['autogenerate_title'])) {
    $product->title = $product_values['title'];
  }
  else {
    // Generate the product title. Take the parent entity title as the base.
    $product->title = $form_state['values']['title'];
    $attributes = _inline_entity_form_commerce_product_attributes($product->type);
    if (!empty($attributes)) {
      $wrapper = entity_metadata_wrapper('commerce_product', $product);
      foreach ($attributes as $field_name => $attribute) {
        $attribute_values[] = $wrapper->{$field_name}->label();
      }
      $product->title .= ' (' . implode(', ', $attribute_values) . ')';
    }
  }
}

/**
 * IEF delete form callback: Returns the confirmation message.
 */
function inline_entity_form_commerce_product_delete_form($form, $form_state) {
  $product = $form['#entity'];
  $form['message'] = array(
    '#markup' => '<div>' . t('Are you sure you want to delete %title?', array('%title' => $product->title)) . '</div>',
  );

  return $form;
}

/**
 * Returns a list of field names that are used as attributes for the given
 * product type.
 */
function _inline_entity_form_commerce_product_attributes($type) {
  // Attributes are tied to the commerce_cart module.
  if (!module_exists('commerce_cart')) {
    return array();
  }

  $attributes = array();
  // Loop through all the field instances on that product type.
  foreach (field_info_instances('commerce_product', $type) as $name => $instance) {
    // A field qualifies if it is single value, required and uses a widget
    // with a definite set of options. For the sake of simplicity, this is
    // currently restricted to fields defined by the options module.
    $field = field_info_field($instance['field_name']);

    // Get the array of Cart settings pertaining to this instance.
    $commerce_cart_settings = commerce_cart_field_instance_attribute_settings($instance);

    // If the instance is of a field type that is eligible to function as
    // a product attribute field and if its attribute field settings
    // specify that this functionality is enabled...
    if (commerce_cart_field_attribute_eligible($field) && $commerce_cart_settings['attribute_field']) {
      $attributes[$name] = array(
        'field' => $field,
        'instance' => $instance,
        'weight' => $instance['widget']['weight'],
      );
    }
  }

  // Sort the fields by weight.
  uasort($attributes, 'drupal_sort_weight');

  return $attributes;
}