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

conf_kcp_t*
conf_create(int argc, const char *argv[]) {

   if (argc > 1) {
      conf_kcp_t *conf = new conf_kcp_t;

      conf->nodelay = 0;
      conf->interval = 20;
      conf->resend = 0;
      conf->nc = 0;      
      conf->kcpconv = 0x28364597;

      for (int i=1; i<argc; i+=2) {

         string opt = argv[i];
         string value = argv[i+1];

         if (opt == "-src_ip") {
            strncpy(conf->src_ip, value.c_str(), 16);
         }

         if (opt == "-src_port") {
            conf->src_port = atoi(value.c_str());
         }

         if (opt == "-dest_ip") {
            strncpy(conf->dest_ip, value.c_str(), 16);
         }

         if (opt == "-dest_port") {
            conf->dest_port = atoi(value.c_str());
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

      // print conf
      cout << "src_ip: " << conf->src_ip << endl;
      cout << "src_port: " << conf->src_port << endl;

      cout << "dest_ip: " << conf->dest_ip << endl;
      cout << "dest_port: " << conf->dest_port << endl;

      cout << "nodelay: " << conf->nodelay << endl;
      cout << "interval: " << conf->interval << endl;

      cout << "resend: " << conf->resend << endl;
      cout << "nc: " << conf->nc << endl;

      cout << "kcpconv: 0x" << hex << conf->kcpconv << endl;

      return conf;
   }
   return NULL;
}

void
conf_release(conf_kcp_t *conf) {
   if (conf) {
      delete conf;
   }
}
