/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef CONF_KCP
#define CONF_KCP

typedef struct {
   char src_ip[16];             // src ip addr
   int src_port;                // src port

   char dest_ip[16];            // dest ip addr
   int dest_port;               // dest port

   int rcv_wndsize;             // receive window size
   int snd_wndsize;             // send window size

   int nodelay;                 // nodelay mode
   int interval;                // million second
   int resend;                  // skip ACKs to resend
   int nc;                      // flow control
   int mtu;                     //

   int fast;                    // fast mode

   int crypto;                  // enable RC4 crytpo
   char key[32];                // secret

   int verbose;                 // support verbose
} conf_kcp_t;

conf_kcp_t* conf_create(int argc, const char *argv[]);
void conf_release(conf_kcp_t*);

#define MKCP_BUF_SIZE 65536     // buffer size
#define MKCP_OVERHEAD 24        // kcp header

static inline int _MIN_OF(int a, int b) {
   return a < b ? a : b;
}

#endif  /* CONF_KCP */
