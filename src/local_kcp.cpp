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
#include "session_kcp.h"

#include <iostream>

using namespace std;

#ifdef LOCAL_KCP

typedef struct {
   chann_t *udpout;
   ikcpcb *kcpout;
   chann_t *tcpin;

   conf_kcp_t *conf;

   unsigned char buf[MNET_BUF_SIZE];

   int is_init;
} tun_local_t;


//
// tcp internal
static void _local_tcpin_listen(chann_event_t *e);
static void _local_tcpin_callback(chann_event_t *e);
static void _local_udpout_callback(chann_event_t *e);
static int _local_kcpout_callback(const char *buf, int len, ikcpcb *kcp, void *user);


// 
// udp & kcp
static int
_local_udpout_create(tun_local_t *tun) {
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
_local_kcpout_create(tun_local_t *tun) {
   tun->kcpout = ikcp_create(tun->conf->kcpconv, tun);
   if (tun->kcpout == NULL) {
      cerr << "Fail to create kcp out !" << endl;
      return 0;
   }

   ikcp_setoutput(tun->kcpout, _local_kcpout_callback);
   ikcp_nodelay(tun->kcpout, tun->conf->nodelay, tun->conf->interval, tun->conf->resend, tun->conf->nc);
   ikcp_wndsize(tun->kcpout, tun->conf->snd_wndsize, tun->conf->rcv_wndsize);
   return 1;
}

static void
_local_kcpout_destroy(tun_local_t *tun) {
   if (tun->kcpout) {
      ikcp_release(tun->kcpout);
      tun->kcpout = NULL;
   }
}


// 
// initial
static int
_local_network_init(tun_local_t *tun) {
   if (tun && !tun->is_init) {
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

      if ( !_local_udpout_create(tun) ) {
         return 0;
      }
      
      if ( !_local_kcpout_create(tun) ) {
         return 0;
      }

      {
         unsigned char buf[16] = {0};
         if ( session_mark_cmd(buf, 0, SESSION_CMD_CONNECT) ) {
            ikcp_send(tun->kcpout, (const char*)buf, 16);
         }
      }

      tun->is_init = 1;
      return 1;
   }
   return 0;
}

static int
_local_network_fini(tun_local_t *tun) {
   if (tun && tun->is_init) {
      _local_kcpout_destroy(tun);
      mnet_fini();
   }
   return 0;
}

static void
_local_network_runloop(tun_local_t *tun) {

   for (;;) {
      IUINT32 current = (IUINT32)(mtime_current() >> 5);

      if (mnet_chann_state(tun->tcpin) == CHANN_STATE_CONNECTED) {

         IUINT32 nextTime = ikcp_check(tun->kcpout, current);
         if (nextTime <= current) {
            ikcp_update(tun->kcpout, current);
         }

         int ret = 0;
         if (ikcp_peeksize(tun->kcpout) > 0) {
            do {
               ret = ikcp_recv(tun->kcpout, (char*)tun->buf, MNET_BUF_SIZE);
               if (ret > 0) {
                  session_kcp_t se;
                  if ( session_probe(tun->buf, ret, &se) ) {
                     ret = mnet_chann_send(tun->tcpin, se.u.data, se.data_length);
                     if (ret < 0) {
                        cerr << "ikcp recv then fail to send " << ret << endl;
                     }
                  } else {
                     cerr << "ikcp recv invalid session " << endl;
                     break;
                  }
               }
            } while (ret > 0);
         }
      }

      mnet_poll( 5000 );        // micro seconds
   }
}

// 
// tcp in
void
_local_tcpin_listen(chann_event_t *e) {
   if (e->event == MNET_EVENT_ACCEPT) {
      tun_local_t *tun = (tun_local_t*)e->opaque;
      if ( tun ) {
         if ( !tun->tcpin ) {

            unsigned char buf[16] = { 0 };
            if ( session_mark_cmd(buf, 0, SESSION_CMD_RESET) ) {
               ikcp_send(tun->kcpout, (const char*)buf, 16);
            }

            tun->tcpin = e->r;
            mnet_chann_set_cb(e->r, _local_tcpin_callback, e->opaque);
            cout << "accept tcpin: " << e->r << endl;
            return;
         }
      }

      mnet_chann_disconnect(e->r);
   }
}

void
_local_tcpin_callback(chann_event_t *e) {
   tun_local_t *tun = (tun_local_t*)e->opaque;

   switch (e->event) {

      case MNET_EVENT_RECV: {
         const int mss = tun->kcpout->mss;
         long chann_ret = 0;
         do {
            int offset = session_mark_data(tun->buf, 0);
            chann_ret = mnet_chann_recv(e->n, &tun->buf[offset], mss - offset);
            if (chann_ret > 0) {
               int kcp_ret = ikcp_send(tun->kcpout, (const char*)tun->buf, chann_ret + offset);
               if (kcp_ret < 0) {
                  cerr << "Fail to send kcp " << kcp_ret << endl;
               }
            }
         } while (chann_ret > 0);
         break;
      }

      case MNET_EVENT_ERROR: {
         cerr << "local error: " << e->err << endl;
         break;
      }

      case MNET_EVENT_DISCONNECT:  {
         cout << "local tcp error or disconnect !" << endl;
         mnet_chann_disconnect(tun->tcpin);
         tun->tcpin = NULL;
         break;
      }

      default: {
         break;
      }
   }
}

// 
// udp callback
void
_local_udpout_callback(chann_event_t *e) {
   tun_local_t *tun = (tun_local_t*)e->opaque;   

   switch (e->event) {

      case MNET_EVENT_RECV: {
         long ret = mnet_chann_recv(e->n, tun->buf, MNET_BUF_SIZE);
         if (ret > 0) {
            ikcp_input(tun->kcpout, (const char*)tun->buf, ret);
         }
         break;
      }

      case MNET_EVENT_DISCONNECT:  {
         cout << "local udp disconnect !" << endl;
         break;
      }

      default: {
         break;
      }
   }
}

//
// kcp callback
int
_local_kcpout_callback(const char *buf, int len, ikcpcb *kcp, void *user) {
   tun_local_t *tun = (tun_local_t*)user;
   if (tun && mnet_chann_state(tun->udpout) >= CHANN_STATE_CONNECTED) {
      return mnet_chann_send(tun->udpout, (void*)buf, len);
   }
   return 0;
}

int
main(int argc, const char *argv[]) {

   tun_local_t *tun = new tun_local_t;
   if ( tun ) {

      tun->conf = conf_create(argc, argv);
      if ( tun->conf ) {

         if ( _local_network_init(tun) ) {
            
            _local_network_runloop(tun);

            _local_network_fini(tun);
         }
         
         conf_release(tun->conf);
      }

      delete tun;
   }

   return 0;
}

#endif
