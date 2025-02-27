#include "encoder.h"

const char *pattern = "\\(\\s*'([^']+)'\\s*,\\s*'([0-9]+\\.[0-9]+)'\\s*\\)";

int base64_encode(char *input, int length)
{
    BIO *b64, *bmem;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);

    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char *encoded_data = (char *)malloc(bptr->length + 1);
    memcpy(encoded_data, bptr->data, bptr->length);
    encoded_data[bptr->length] = '\0';

    BIO_free_all(b64);

    return 0;
}

int base64_decode(char *input, int length, int *out_len)
{
    BIO *b64, *bmem;
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new_mem_buf((void *) input, length);
    bmem = BIO_push(b64, bmem);

    *out_len = BIO_read(bmem, input, length);
    BIO_free_all(bmem);

	return 0;
}

static void save_json(char *json_str, char keys[][MAX_KEY_SIZE], 
		char values[][MAX_VALUE_SIZE], int count)
{
	memset(json_str, 0, MAX_JSON_SIZE);

    strcat(json_str, "{\n");
    for (int i = 0; i < count; i++) {
        strcat(json_str, "  \"");
        strcat(json_str, keys[i]);
        strcat(json_str, "\": \"");
        strcat(json_str, values[i]);
        strcat(json_str, "\"");

        if (i < count - 1) {
            strcat(json_str, ",");
        }

        strcat(json_str, "\n");
    }

    strcat(json_str, "}\n");
}

int json_encode(char *input)
{
	regex_t regex;
	regmatch_t matches[MAX_MATCHES];
	char keys[MAX_MATCHES][MAX_KEY_SIZE];
	char values[MAX_MATCHES][MAX_VALUE_SIZE];
	int match_count;
	int ret;

	ret = regcomp(&regex, pattern, REG_EXTENDED);
	if (ret) {
		printf("regcomp failed\n");
		return -1;
	}

	const char *cursor = input;
	match_count = 0;
	while (!regexec(&regex, cursor, MAX_MATCHES, matches, 0)) {
		int key_start = matches[1].rm_so;
		int key_end = matches[1].rm_eo;
		int key_len = key_end - key_start;

		strncpy(keys[match_count], cursor + key_start, key_len);
		keys[match_count][key_len] = '\0';

		int value_start = matches[2].rm_so;
		int value_end = matches[2].rm_eo;
		int value_len = value_end - value_start;

		strncpy(values[match_count], cursor + value_start, value_len);
		values[match_count][value_len] = '\0';

		cursor += matches[0].rm_eo;
		match_count++;
	}

	save_json(input, keys, values, match_count);

	regfree(&regex);

	return 0;
}

int json_decode(char *input, char *model, char *image)
{
	struct json_object *json;
	struct json_object *json_model;
	struct json_object *json_image;
	int ret;

	json = json_tokener_parse(input);
	if (!json) {
		printf("json_tokener_parse failed\n");
		return -1;
	}

	ret = json_object_object_get_ex(json, "model_name", &json_model);
	if (ret) {
		const char *model_name = json_object_get_string(json_model);
		strncpy(model, model_name, strlen(model_name));
		model[strlen(model_name) - 1] = '\0';
	} else {
		printf("Missing 'model_name' in JSON\n");
		return -1;
	}
	//print("model name:%s\n", model);

	ret = json_object_object_get_ex(json, "image_data", &json_image);
	if (ret) {
		const char *image_data = json_object_get_string(json_image);
		strncpy(model, image_data, strlen(image_data));
		model[strlen(image_data) - 1] = '\0';
	} else {
		printf("Missing 'image_data' in JSON\n");
		return -1;
	}
	//print("model name:%s\n", model);

	return 0;
}
