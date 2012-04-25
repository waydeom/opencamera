#ifndef _RSA_CRYPTO_H_
#define _RSA_CRYPTO_H_

#include "e_os.h"

#include "crypto.h"
#include "err.h"
//#include "rand.h"
#include "bn.h"
#include "rsa.h"

const unsigned char crypto_common[] = "\x11";

char* get_public_key();
void* init_crypto_key(unsigned char* pub_key, unsigned int pub_key_len, unsigned char* priv_key, unsigned int priv_key_len);
int  crypto_encode(void* handle, unsigned char* input, unsigned int input_length,
					unsigned char* output, unsigned int output_length);
int  crypto_decode(void* handle, unsigned char* input, unsigned int input_length,
					unsigned char* output, unsigned int output_length);
void  release_crypto_key(void* handle);

int hex2array(char* hex, int hex_len, unsigned char* array, int array_len);
int array2hex(unsigned char* array, int array_len, char* hex, int hex_len);
void readkey(char *chCode);
void writekey(char const *chCode);

#endif
