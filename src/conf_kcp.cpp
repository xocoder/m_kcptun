// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 
#define _CRT_SECURE_NO_WARNINGS
#include "mnet_core.h"
#include "conf_kcp.h"
#include "m_mem.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

static bool
_str_empty(const char *str) {
   if (str==NULL || str[0]=='\0') {
      return true;
   } else {
      return false;
   }
}

conf_kcp_t*
conf_create(int argc, const char *argv[]) {

   if (argc > 1) {
      conf_kcp_t *conf = new conf_kcp_t;
      memset(conf, 0, sizeof(*conf));

      conf->rs_data = 11;       // RS(11, 2)
      conf->rs_parity = 2;
      conf->mtu = 1400;
      conf->rcv_wndsize = 256;
      conf->snd_wndsize = 256;


      for (int i=1; i<argc; i+=2) {

         chann_addr_t addr;
         string opt = argv[i];
         string value = (argc - i) > 1 ? argv[i+1] : "";

         if (conf->src_count<4 && opt=="-l") {
            int i = conf->src_count;
            mnet_parse_ipport((char*)value.c_str(), &addr);
            strncpy(conf->src_ip[i], addr.ip, sizeof(addr.ip));
            conf->src_port[i] = addr.port;
            conf->src_count = i + 1;
         }

         if (conf->dest_count<4 && opt=="-r") {
            int i = conf->dest_count;
            mnet_parse_ipport((char*)value.c_str(), &addr);
            strncpy(conf->dest_ip[i], addr.ip, sizeof(addr.ip));
            conf->dest_port[i] = addr.port;
            conf->dest_count = i + 1;
         }

         if (opt == "-nodelay") {
            conf->nodelay = atoi(value.c_str());
         }

         if (opt == "-interval") {
            conf->interval = atoi(value.c_str());
         }

         if (opt == "-resend") {
            conf->resend = atoi(value.c_str());
         }

         if (opt == "-nc") {
            conf->nc = atoi(value.c_str());
         }

         if (opt == "-rcv_wndsize") {
            conf->rcv_wndsize = atoi(value.c_str());
         }

         if (opt == "-snd_wndsize") {
            conf->snd_wndsize = atoi(value.c_str());
         }

         if (opt == "-mtu") {
            conf->mtu = atoi(value.c_str());
         }

         if (opt == "-fast") {
            conf->fast = atoi(value.c_str());
         }

         if (opt == "-rs_data") {
            conf->rs_data = atoi(value.c_str());
         }

         if (opt == "-rs_parity") {
            conf->rs_parity = atoi(value.c_str());
         }

         if (opt == "-h" || opt == "-help") {
            goto usage;
         }

         if (opt == "-key") {
            conf->crypto = 1;
            snprintf(conf->key, 32, "%s", value.c_str());
         }

         if (opt == "-tcp_cache") {
            conf->tcp_cache = atoi(value.c_str());
         }

         if (opt == "-v" || opt == "-version") {
            cerr << "mkcptun: v20180609" << endl;
            return NULL;
         }
      }

      if (conf->tcp_cache <= 0) {
         conf->tcp_cache = 2*1024*1024;
      }

      if (conf->rs_data>0 && conf->rs_parity>0) {
         conf->mtu = 1400 / (conf->rs_data + conf->rs_parity) * conf->rs_data;
      } else {
         conf->rs_data = conf->rs_parity = 0;
      }

      switch (conf->fast) {

         case 3: {
            conf->nodelay = 1;
            conf->interval = 10;
            conf->resend = 2;
            conf->nc = 1;
            conf->rcv_wndsize = 1024;
            conf->snd_wndsize = 1024;
            break;
         }

         case 2: {
            conf->nodelay = 1;
            conf->interval = 20;
            conf->resend = 4;
            conf->nc = 1;
            conf->rcv_wndsize = 512;
            conf->snd_wndsize = 512;
            break;
         }

         case 1: {
            conf->nodelay = 1;
            conf->interval = 40;
            conf->resend = 0;
            conf->nc = 1;
            conf->rcv_wndsize = 256;
            conf->snd_wndsize = 256;
            break;
         }

         default: {
            conf->nodelay = 0;
            conf->interval = 100;
            conf->resend = 0;
            conf->nc = 0;
            break;
         }
      }

      if (!_str_empty(conf->src_ip[0]) &&
          !_str_empty(conf->dest_ip[0]) &&
          conf->src_port[0] > 0 &&
          conf->dest_port[0] > 0)
      {
         // print conf
         cout << "---------- begin config ----------" << endl;
         for (int i=0; i<conf->src_count; i++) {
            cout << "listen: " << conf->src_ip[i] << ":" << conf->src_port[i] << endl;
         }
         for (int i=0; i<conf->dest_count; i++) {
            cout << "remote: " << conf->dest_ip[i] << ":" << conf->dest_port[i] << endl;
         }

         cout << "nodelay: " << conf->nodelay << endl;
         cout << "interval: " << conf->interval << endl;

         cout << "resend: " << conf->resend << endl;
         cout << "nc: " << conf->nc << endl;

         if (conf->rs_data) {
            cout << "mtu (with rs): " << conf->mtu << endl;
         } else {
            cout << "mtu: " << conf->mtu << endl;
         }
         cout << "crypto: " << conf->crypto << endl;

         cout << "rcv_wndsize: " << conf->rcv_wndsize << endl;
         cout << "snd_wndsize: " << conf->snd_wndsize << endl;
         
         cout << "fast: " << conf->fast << endl;         
         cout << "rs_data: " << conf->rs_data << endl;
         cout << "rs_parity: " << conf->rs_parity << endl;

#ifdef KCP_REMOTE
         cout << "tcp_cache: " << conf->tcp_cache << endl;
#endif
         cout << "---------- end config ----------" << endl;
         return conf;
      } else {
         cout << "invalid " << conf->src_ip << conf->src_port << endl;
      }

      delete conf;
   }

  usage:
   cerr << "Usage:" << endl;

#ifdef KCP_LOCAL
   cerr << argv[0] << ": -l LISTEN_IP:PORT -r REMOTE_IP_1:PORT -r REMOTE_IP_2:PORT -fast 3 -key 'SECRET_256bits'" << endl;
#else
   cerr << argv[0] << ": -l LISTEN_IP_1:PORT -l LISTEN_IP_2:PORT -t REMOTE_IP:PORT -fast 3 -key 'SECRET_256bits'" << endl;   
#endif

   cerr << "Optinal: \t support IP count " << IP_COUNT << endl;
   cerr << "-key     \t set communication secret" << endl;
   cerr << "-nodelay \t whether nodelay mode is enabled, 0 is not enabled; 1 enabled" <<endl;
   cerr << "-interval \t protocol internal work interval, in milliseconds" << endl;
   cerr << "-resend \t fast retransmission mode, 0 represents off by default, 2 can be set" << endl;
   cerr << "        \t (2 ACK spans will result in direct retransmission)" << endl;
   cerr << "-nc     \t whether to turn off flow control, 1 represents no flow control" << endl;
   cerr << "-snd_wndsize \t send window size" << endl;
   cerr << "-rcv_wndsize \t recv window size" << endl;
   cerr << "-fast   \t fast mode with pre defined option:" << endl;
   cerr << "        \t 3 most fast, with ikcp_nodelay  \t [1 10 2 1]" << endl;
   cerr << "        \t 2 mid fast, with ikcp_nodelay   \t [1 20 4 1]" << endl;
   cerr << "        \t 1 least fast, with ikcp_nodelay \t [1 40 0 1]" << endl;
   cerr << "        \t 0 default, with ikcp_nodelay    \t [0 100 0 0]" << endl;
   cerr << "-rs_data \t Reed-Solomon erasure codes data bytes, default 11" << endl;
   cerr << "-rs_parity \t Reed-Solomon erasure codes parity bytes, default 2" << endl;
#ifdef KCP_REMOTE
   cerr << "-tcp_cache \t TCP cache size, default 2*1024*1024" << endl;
#endif
   cerr << "-help   \t print this help" << endl;
   cerr << "-version \t print version" << endl;
   return NULL;
}

void
conf_release(conf_kcp_t *conf) {
   if (conf) {
      delete conf;
   }
}

void* kcp_malloc(size_t size) {
   return mm_malloc(size);
}

void kcp_free(void *p) {
   mm_free(p);
}
