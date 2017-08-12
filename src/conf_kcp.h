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

   int kcpconv;                 // kcp context
   int verbose;                 // support verbose
} conf_kcp_t;

conf_kcp_t* conf_create(int argc, const char *argv[]);
void conf_release(conf_kcp_t*);

#endif  /* CONF_KCP */
