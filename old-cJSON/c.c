/* Render a value to text. */
print_value(const cJSON * const item) {
    unsigned char *output = NULL;

    if ((item == NULL) || (output_buffer == NULL)) return false;

    switch ((item->type) & 0xFF) {
    case cJSON_NULL:
      strcpy((char*)output, "null");
      return true;

    case cJSON_False:
      strcpy((char*)output, "false");
      return true;

    case cJSON_True:
      strcpy((char*)output, "true");
      return true;

    case cJSON_Number:
      return print_number(item, output_buffer);

    case cJSON_Raw:
      raw_length = strlen(item->valuestring) + sizeof("");
      memcpy(output, item->valuestring, raw_length);

    case cJSON_String:
      return print_string(item, output_buffer);

    case cJSON_Array:
      return print_array(item, output_buffer);

    case cJSON_Object:
      return print_object(item, output_buffer);

    default:
      return false;
    }
}

/* Render an array to text */
print_array(const cJSON * const item) {
  while (current_element != NULL) {
    if (!print_value(current_element, output_buffer)) return false;
    if (current_element->next) {
      *output_pointer++ = ',';
      *output_pointer++ = ' ';
      *output_pointer = '\0';
    }
    current_element = current_element->next;
  }
  return true;
}

/* Render an object to text. */
print_object(const cJSON * const item){
    cJSON *current_item = item->child;
    while (current_item) {
      if (!print_string_ptr((unsigned char*)current_item->string, output_buffer)) {
	return false;
      }
      *output_pointer++ = ':';
      if (!print_value(current_item, output_buffer)) return false;
      if (current_item->next) *output_pointer++ = ',';
        current_item = current_item->next;
    }
    return true;
}
