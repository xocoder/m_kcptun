/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef CONF_KCP_H
#define CONF_KCP_H

#include "stdlib.h"
#include "m_mem.h"

#define IP_COUNT 8              // UDP addr for transport KCP

typedef struct {
   int src_count;               // UDP addr count
   char src_ip[IP_COUNT][16];   // src ip addr
   int src_port[4];             // src port

   int dest_count;              // UDP addr count   
   char dest_ip[IP_COUNT][16];  // dest ip addr
   int dest_port[4];            // dest port

   int rcv_wndsize;             // receive window size
   int snd_wndsize;             // send window size

   int nodelay;                 // nodelay mode
   int interval;                // million second
   int resend;                  // skip ACKs to resend
   int nc;                      // flow control
   int mtu;                     //

   int fast;                    // fast mode
   int crypto;                  // enable RC4 crytpo

   int rs_data;                 // data bytes for reed solomon codec 
   int rs_parity;               // parity bytes for reed solomon codec    
   char key[32];                // secret 256bits
} conf_kcp_t;

conf_kcp_t* conf_create(int argc, const char *argv[]);
void conf_release(conf_kcp_t*);

#define MKCP_BUF_SIZE 65536     // buffer size
#define MKCP_OVERHEAD 24        // kcp header

static inline void* kcp_malloc(size_t size) {
   return mm_malloc(size);
}

static inline void kcp_free(void *p) {
   mm_free(p);
}

#endif  /* CONF_KCP */
