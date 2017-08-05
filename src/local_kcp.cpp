// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#include "ikcp.h"
#include "plat_net.h"

#include "conf_kcp.h"
#include "kcp_private.h"

#include <list>
#include <iostream>

using namespace std;

#ifdef LOCAL_KCP

typedef struct {
   chann_t *udpout;
   ikcpcb *kcpout;

   conf_kcp_t *conf;
   list<chann_t*> tcpinList;
} tun_local_t;



//
// kcp intervace
static void _local_tcpin_listen(chann_event_t *e);
static void _local_tcpin_callback(chann_event_t *e);
static void _local_udpout_callback(chann_event_t *e);


// 
// initial
static int
_local_network_init(tun_local_t *tun) {
   mnet_init();

   chann_t *tcp = mnet_chann_open(CHANN_TYPE_STREAM);
   if (tcp == NULL) {
      cerr << "Fail to create listen tcp !" << endl;
      return 0;
   }

   mnet_chann_set_cb(tcp, _local_tcpin_listen, tun);
   if (mnet_chann_listen_ex(tcp, tun->conf->src_ip, tun->conf->src_port, 1) <= 0) {
      cerr << "Fail to listen tcpin !" << endl;
      return 0;
   }

   tun->udpout = mnet_chann_open(CHANN_TYPE_DGRAM);
   if (tun->udpout == NULL) {
      cerr << "Fail to create udp out !" << endl;
      return 0;
   }

   mnet_chann_set_cb(tun->udpout, _local_udpout_callback, tun);
   if (mnet_chann_to(tun->udpout, tun->conf->dest_ip, tun->conf->dest_port) <= 0) {
      cerr << "Fail to connect to udp out !" << endl;
      return 0;
   }

   return 1;
}

static int
_local_network_fini(tun_local_t *tun) {

   mnet_fini();
   return 0;
}

// 
// tcp in
void
_local_tcpin_listen(chann_event_t *e) {
   if (e->event == MNET_EVENT_ACCEPT) {
      tun_local_t *tun = (tun_local_t*)e->opaque;
      tun->tcpinList.push_back(e->r);
      mnet_chann_set_cb(e->r, _local_tcpin_callback, e->opaque);
   }
}

void
_local_tcpin_callback(chann_event_t *e) {
   tun_local_t *tun = (tun_local_t*)e->opaque;

   if (e->event == MNET_EVENT_DISCONNECT) {
      tun->tcpinList.remove(e->n);
   }
}

// 
// udp out
void
_local_udpout_callback(chann_event_t *e) {
}

// 
// runloop
static void
_local_runloop(tun_local_t *tun) {   
}

int
main(int argc, const char *argv[]) {
   tun_local_t tun;
   memset(&tun, 0, sizeof(tun));

   tun.conf = conf_from_argv(argc, argv);
   if ( !tun.conf ) {
      cerr << "Fail to read conf from stdin !" << endl;
      return 0;
   }

   _local_network_init(&tun);

   // init tcpin
   // init kcpout

   return 0;
}

#endif
