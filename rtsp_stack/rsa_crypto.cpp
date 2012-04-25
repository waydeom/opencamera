#include "rsa_crypto.h"
#include <stdio.h>
#include <cstring>
#include "Debug.h"
// This function should be rewritten on a specific platform

char* get_public_key()
{
	return NULL;
}


void* init_crypto_key(unsigned char* pub_key, unsigned int pub_key_len, unsigned char* priv_key, unsigned int priv_key_len)
{
	RSA* rsa = RSA_new();
	unsigned char* pub, *priv;
	pub = new unsigned char[pub_key_len/2];
	priv = new unsigned char[priv_key_len/2];
	hex2array((char*)pub_key, pub_key_len, pub, pub_key_len/2);
	hex2array((char*)priv_key, priv_key_len, priv, priv_key_len/2);
	rsa->n = BN_bin2bn(pub, pub_key_len/2, rsa->n);
	rsa->d = BN_bin2bn(priv, priv_key_len/2, rsa->d);
	rsa->e = BN_bin2bn(crypto_common, 1, rsa->e);
	delete [] pub;
	delete [] priv;
	return rsa;
}

int  crypto_encode(void* handle, unsigned char* input, unsigned int input_length,
					unsigned char* output, unsigned int output_length)
{
	RSA* rsa = (RSA*)handle;
	unsigned char* pack_output = new unsigned char[output_length];
	
	int num = RSA_public_encrypt(input_length, input, pack_output, rsa,
		RSA_PKCS1_PADDING);
	if (num > 0)
	{
		num = array2hex(pack_output, num, (char*)output, output_length);
	}
	else if (num == -1)
	{
	    unsigned long err = ERR_get_error();
		Debug(ckite_log_message, "Error string %s\n", ERR_error_string(err, NULL));
	}
	delete [] pack_output;
	return num;
}

int  crypto_decode(void* handle, unsigned char* input, unsigned int input_length,
					unsigned char* output, unsigned int output_length)
{
	RSA* rsa = (RSA*)handle;
	unsigned char* pack_input = new unsigned char[input_length/2];
	hex2array((char*)input, input_length, pack_input, input_length/2);
	int num = RSA_private_decrypt(input_length/2, pack_input, output, rsa,
		RSA_PKCS1_PADDING);
	
	return num;
}

void  release_crypto_key(void* handle)
{
	RSA* rsa = (RSA*)handle;
	delete rsa;
}

int hex_digit(unsigned char hex)
{
	if (hex >= '0' && hex <= '9')
		return hex - '0';
	if (hex >= 'A' && hex <= 'F')
		return hex - 'A' + 10;
	if (hex >= 'a' && hex <= 'f')
		return hex - 'a' + 10;
	return 0;
}

char hex_value(unsigned int digit)
{
	if (digit >= 0 && digit <= 9)
		return '0' + digit;
	else if (digit >= 10 && digit <= 15)
		return 'A' + (digit-10);
	else
		return '0';
}

int hex2array(char* hex, int hex_len, unsigned char* array, int array_len)
{
	int i;
	if (array_len < hex_len/2)
		return -1;
	if (hex_len & 1)	// hex_len must be even!
		return -1;
	for (i=0; i<hex_len; i+=2)
	{
		array[i/2] = hex_digit(hex[i])*16 + hex_digit(hex[i+1]);
	}
	return hex_len/2;
}

int array2hex(unsigned char* array, int array_len, char* hex, int hex_len)
{
	int i;
	if (hex_len < array_len*2)
		return -1;
	for (i=0; i<array_len; i++)
	{
		hex[2*i] = hex_value(array[i] >> 4);
		hex[2*i+1] = hex_value(array[i] & 0x0f);
	}
	return 2*array_len;
}
