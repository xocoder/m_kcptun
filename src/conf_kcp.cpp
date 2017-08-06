// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#include "conf_kcp.h"
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

      conf->nodelay = 0;
      conf->interval = 20;
      conf->resend = 0;
      conf->nc = 0;      
      conf->kcpconv = 0x28364597;

      for (int i=1; i<argc; i+=2) {

         string opt = argv[i];
         string value = argv[i+1];

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

         if (opt == "-target" || opt == "-t") {
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

         if (opt == "-kcpconv") {
            conf->kcpconv = atoi(value.c_str());
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

         cout << "kcpconv: 0x" << hex << conf->kcpconv << endl;
         cout << "---------- end config ----------" << dec << endl;

         return conf;
      }

      delete conf;
   }

   cerr << argv[0] << ": -src_ip IP -src_port PORT -dest_ip IP -dest_port PORT [-nodelay | -interval | -resend | -nc | -kcpconv]" << endl;
   return NULL;
}

void
conf_release(conf_kcp_t *conf) {
   if (conf) {
      delete conf;
   }
}
