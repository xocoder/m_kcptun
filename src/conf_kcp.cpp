// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#include "conf_kcp.h"
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

      conf->kcpconv = 0x28364597;

      conf->mtu = 1400;
      conf->rcv_wndsize = 32;
      conf->snd_wndsize = 32;

      for (int i=1; i<argc; i+=2) {

         string opt = argv[i];
         string value = (argc - i) > 1 ? argv[i+1] : "";

         if (opt == "-listen" || opt == "-l") {
            int f = value.find(":");
            if (f > 0) {
               strncpy(conf->src_ip, value.substr(0, f).c_str(), 16);
               conf->src_port = atoi(value.substr(f+1, value.length() - f).c_str());
            } else if (f==0 && value.length() > 1) {
               strncpy(conf->src_ip, "0.0.0.0", 7);
               conf->src_port = atoi(value.substr(f+1, value.length() - f).c_str());
            }
         }

         if (opt == "-target" || opt == "-t" || opt == "-r") {
            int f = value.find(":");
            if (f > 0) {
               strncpy(conf->dest_ip, value.substr(0, f).c_str(), 16);
               conf->dest_port = atoi(value.substr(f+1, value.length() - f).c_str());
            }
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

         if (opt == "-verbose" || opt == "-v") {
            conf->verbose = 1;
            i -= 1;
         }

         if (opt == "-h" || opt == "-help") {
            goto usage;
         }

         if (opt == "-key") {
            conf->crypto = 1;
            snprintf(conf->key, 32, "%s", value.c_str());
         }

         if (opt == "-v" || opt == "-version") {
            cerr << "mkcptun: v20170815" << endl;
            return NULL;
         }
      }

      switch (conf->fast) {

         case 3: {
            conf->nodelay = 1;
            conf->interval = 10;
            conf->resend = 2;
            conf->nc = 1;
            break;
         }

         case 2: {
            conf->nodelay = 1;
            conf->interval = 20;
            conf->resend = 4;
            conf->nc = 1;
            break;
         }

         case 1: {
            conf->nodelay = 1;
            conf->interval = 40;
            conf->resend = 0;
            conf->nc = 1;
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

      if (!_str_empty(conf->src_ip) &&
          !_str_empty(conf->dest_ip) &&
          conf->src_port > 0 &&
          conf->dest_port > 0)
      {
         // print conf
         cout << "listen: " << conf->src_ip << ":" << conf->src_port << endl;
         cout << "target: " << conf->dest_ip << ":" << conf->dest_port << endl;

         cout << "nodelay: " << conf->nodelay << endl;
         cout << "interval: " << conf->interval << endl;

         cout << "resend: " << conf->resend << endl;
         cout << "nc: " << conf->nc << endl;

         cout << "mtu: " << conf->mtu << endl;
         cout << "crypto: " << conf->crypto << endl;

         cout << "rcv_wndsize: " << conf->rcv_wndsize << endl;
         cout << "snd_wndsize: " << conf->snd_wndsize << endl;

         cout << "fast: " << conf->fast << endl;
         cout << "---------- end config ----------" << endl;

         return conf;
      }

      delete conf;
   }

  usage:
   cerr << "Usage:" << endl;
   cerr << argv[0] << ": -l LISTEN_IP:PORT -t TARGET_IP:PORT -fast 3 -key 'SECRET'" << endl << endl;

   cerr << "Optinal:" << endl;
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
   cerr << "-verbose \t verbose output" << endl;
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
