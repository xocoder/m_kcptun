/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef CRYPTO_KCP_H
#define CRYPTO_KCP_H

#include <time.h>

uint64_t rc4_hash_key(const char * str, int sz);

int rc4_encrypt(const char *in, int sz, char *out, uint64_t key, time_t ti);
int rc4_decrypt(const char *in, int sz, char *out, uint64_t key, time_t ti);

#endif
