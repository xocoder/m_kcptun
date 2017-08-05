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

         if (opt == "-srcip") {
            strncpy(conf->src_ip, argv[i+1], 16);
         }

         if (opt == "-srcport") {
            conf->src_port = stoi(value);
         }

         if (opt == "-destip") {
            strncpy(conf->dest_ip, argv[i+1], 16);
         }

         if (opt == "-destport") {
            conf->dest_port = stoi(value);
         }

         if (opt == "-nodelay") {
            conf->nodelay = stoi(value);
         }

         if (opt == "-interval") {
            conf->interval = stoi(value);
         }

         if (opt == "-resend") {
            conf->resend = stoi(value);
         }

         if (opt == "-nc") {
            conf->nc = stoi(value);
         }

         if (opt == "-kcpconv") {
            conf->kcpconv = stoi(value);
         }
      }      
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
