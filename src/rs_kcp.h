/* 
 * Copyright (c) 2018 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#ifndef RS_KCP_H
#define RS_KCP_H

typedef struct s_rskcp rskcp_t;

rskcp_t* rskcp_create(int data_bytes, int parity_bytes);

void rskcp_release(rskcp_t*);

int rskcp_enc_info(rskcp_t*, int raw_len, int *parity_len);
int rskcp_dec_info(rskcp_t*, int data_len);

int rskcp_encode(rskcp_t*, unsigned char *raw, int raw_len, unsigned char *parity);
int rskcp_decode(rskcp_t*, unsigned char *data, int data_len, unsigned char *parity);

#endif  /* RS_KCP_H */
