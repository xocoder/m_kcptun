// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#include "plat_net.h"
#include "plat_time.h"

#include "ikcp.h"
#include "conf_kcp.h"

#include <iostream>

using namespace std;

#ifdef REMOTE_KCP

typedef struct {
   chann_t *udpin;
   ikcpcb *kcpin;
   chann_t *tcpout;

   conf_kcp_t *conf;

   unsigned char *buf[MNET_BUF_SIZE];

   IUINT32 ti_ms;               // last time to check

   int isInit;
} tun_remote_t;


//
// tcp internal
static void _remote_tcpout_callback(chann_event_t *e);
static void _remote_udpin_callback(chann_event_t *e);
static int _remote_kcpin_callback(const char *buf, int len, ikcpcb *kcp, void *user);



// 
// initial
static int
_remote_network_init(tun_remote_t *tun) {
   if (tun && !tun->isInit) {
      mnet_init();

      tun->tcpout = mnet_chann_open(CHANN_TYPE_STREAM);
      if (tun->tcpout == NULL) {
         cerr << "Fail to create tcp out !" << endl;
         return 0;
      }

      mnet_chann_set_cb(tun->tcpout, _remote_tcpout_callback, tun);
      if (mnet_chann_connect(tun->tcpout, tun->conf->dest_ip, tun->conf->dest_port) <= 0) {
         cerr << "Tcpout fail to connect !" << endl;
         return 0;
      }

      tun->udpin = mnet_chann_open(CHANN_TYPE_DGRAM);
      if (tun->udpin == NULL) {
         cerr << "Fail to create udp in !" << endl;
         return 0;
      }

      mnet_chann_set_cb(tun->udpin, _remote_udpin_callback, tun);
      if (mnet_chann_to(tun->udpin, tun->conf->src_ip, tun->conf->src_port) <= 0) {
         cerr << "Fail to connect to udp in !" << endl;
         return 0;
      }

      // kcp 
      tun->kcpin = ikcp_create(tun->conf->kcpconv, tun);
      if (tun->kcpin == NULL) {
         cerr << "Fail to create kcp in !" << endl;
         return 0;
      }

      ikcp_setoutput(tun->kcpin, _remote_kcpin_callback);
      
      tun->isInit = 1;
      return 1;
   }
   return 0;
}

static int
_remote_network_fini(tun_remote_t *tun) {
   if (tun) {
      ikcp_release(tun->kcpin);
      mnet_fini();
   }
   return 0;
}

static void
_remote_network_runloop(tun_remote_t *tun) {

   for (;;) {
      IUINT32 ti = (IUINT32)mtime_current();
      IUINT32 current = 10;

      if (ti - tun->ti_ms >= 5) {
         current = ikcp_check(tun->kcpin, ti);

         if (current <= ti) {

            ikcp_update(tun->kcpin, current);

            tun->ti_ms = ti;
         }
      }
      
      mnet_poll( (current - ti) | 5 );
   }
}

// 
// tcp out
void
_remote_tcpout_callback(chann_event_t *e) {
   tun_remote_t *tun = (tun_remote_t*)e->opaque;

   switch (e->event) {
      
      case MNET_EVENT_CONNECTED: {
         cout << "Tcp out connected !" << endl;
         break;
      }

      case MNET_EVENT_RECV: {
         long chann_ret = mnet_chann_recv(e->n, tun->buf, MNET_BUF_SIZE);
         if (chann_ret > 0) {
            int kcp_ret = ikcp_send(tun->kcpin, (const char*)tun->buf, chann_ret);
            if (kcp_ret < 0) {
               cerr << "Fail to send kcp " << kcp_ret << endl;
            }
         }
         break;
      }

      case MNET_EVENT_ERROR: {
         cerr << "local error: " << e->err << endl;
         break;
      }

      case MNET_EVENT_DISCONNECT:  {
         cout << "remote tcp error or disconnect !" << endl;
         mnet_chann_disconnect(tun->tcpout);
         tun->tcpout = NULL;
         break;
      }

      default: {
         break;
      }
   }
}

// 
// udp out
void
_remote_udpin_callback(chann_event_t *e) {
   tun_remote_t *tun = (tun_remote_t*)e->opaque;   

   switch (e->event) {

      case MNET_EVENT_RECV: {
         long ret = mnet_chann_recv(e->n, tun->buf, MNET_BUF_SIZE);
         if (ret > 0) {
            ikcp_input(tun->kcpin, (const char*)tun->buf, ret);
         }
         break;
      }

      case MNET_EVENT_DISCONNECT:  {
         cout << "remote udp disconnect !" << endl;
         // FIXME: should not reach here
         break;
      }

      default: {
         break;
      }
   }
}

//
// kcp out
int
_remote_kcpin_callback(const char *buf, int len, ikcpcb *kcp, void *user) {
   tun_remote_t *tun = (tun_remote_t*)user;
   if (tun && mnet_chann_state(tun->tcpout) == CHANN_STATE_CONNECTED) {
      return mnet_chann_send(tun->tcpout, (void*)tun->buf, len);
   }
   return 0;
}

int
main(int argc, const char *argv[]) {

   tun_remote_t *tun = new tun_remote_t;
   if ( tun ) {

      tun->conf = conf_create(argc, argv);
      if ( tun->conf ) {

         if ( _remote_network_init(tun) ) {
            
            _remote_network_runloop(tun);

            _remote_network_fini(tun);
         }

         conf_release(tun->conf);
      } else {
         cerr << argv[0] << ": -src_ip IP -src_port PORT -dest_ip IP -dest_port PORT [-nodelay | -interval | -resend | -nc | -kcpconv]" << endl;
      }

      delete tun;
   }

   return 0;
}

#endif
