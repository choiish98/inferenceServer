#ifndef ENCODER_H
#define ENCODER_H

#include "common.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include <json-c/json.h> 
#include <regex.h>

#define MAX_MATCHES 10
#define MAX_KEY_SIZE 100
#define MAX_VALUE_SIZE 50
#define MAX_JSON_SIZE 2048

int base64_encode(char *input, int length);
int base64_decode(char *input, int length, int *out_len);

int json_encode(char *);
int json_decode(char *, char *, char *);

#endif
