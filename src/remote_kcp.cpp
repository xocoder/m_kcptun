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
#include "session_proto.h"

#include <iostream>

#ifdef REMOTE_KCP

using namespace std;

typedef struct {
   chann_t *udpin;              // listen
   ikcpcb *kcpin;
   chann_t *tcpout;

   conf_kcp_t *conf;

   unsigned char buf[MNET_BUF_SIZE];

   IUINT32 last_ti_try_connect; // last time to check

   int is_init;
} tun_remote_t;


//
// tcp internal
static void _remote_tcpout_callback(chann_event_t *e);
static void _remote_udpin_callback(chann_event_t *e);
static int _remote_kcpin_callback(const char *buf, int len, ikcpcb *kcp, void *user);

static int
_tcpout_open_and_connect(tun_remote_t *tun) {
   tun->tcpout = mnet_chann_open(CHANN_TYPE_STREAM);
   if (tun->tcpout == NULL) {
      cerr << "Fail to create tcp out !" << endl;
      return 0;
   }

   mnet_chann_set_cb(tun->tcpout, _remote_tcpout_callback, tun);
   if (mnet_chann_connect(tun->tcpout, tun->conf->dest_ip, tun->conf->dest_port) <= 0) {
      cerr << "tcp out fail to connect !" << endl;
      return 0;
   } else {
      cout << "tcp out try connect " << endl;
      return 1;
   }
}


// 
// udp & kcp
static int
_remote_kcpin_create(tun_remote_t *tun) {
   tun->kcpin = ikcp_create(tun->conf->kcpconv, tun);
   if (tun->kcpin == NULL) {
      cerr << "Fail to create kcp in !" << endl;
      return 0;
   }

   ikcp_setoutput(tun->kcpin, _remote_kcpin_callback);
   ikcp_wndsize(tun->kcpin, tun->conf->snd_wndsize, tun->conf->rcv_wndsize);
   return 1;
}

static void
_remote_kcpin_destroy(tun_remote_t *tun) {
   if ( tun->kcpin ) {
      ikcp_release(tun->kcpin);
      tun->kcpin = NULL;
   }
}



// 
// initial
static int
_remote_network_init(tun_remote_t *tun) {
   if (tun && !tun->is_init) {
      mnet_init();

      if (_tcpout_open_and_connect(tun) <= 0) {
         return 0;
      }

      tun->udpin = mnet_chann_open(CHANN_TYPE_DGRAM);
      if (tun->udpin == NULL) {
         cerr << "Fail to create udp in !" << endl;
         return 0;
      }

      mnet_chann_set_cb(tun->udpin, _remote_udpin_callback, tun);
      if (mnet_chann_listen_ex(tun->udpin, tun->conf->src_ip, tun->conf->src_port, 1) <= 0) {
         cerr << "Fail to connect to udp in !" << endl;
         return 0;
      }

      // kcp
      if ( !_remote_kcpin_create(tun) ) {
         return 0;
      }
      
      tun->is_init = 1;
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
      int64_t cur_64 = mtime_current();
      IUINT32 current = (IUINT32)(cur_64 >> 5);

      if (mnet_chann_state(tun->tcpout) == CHANN_STATE_CONNECTED) {
      
         IUINT32 nextTime = ikcp_check(tun->kcpin, current);
         if (nextTime <= current) {
            ikcp_update(tun->kcpin, current);
         }


         if (ikcp_peeksize(tun->kcpin) > 0) {
            int ret = 0;
            do {
               ret = ikcp_recv(tun->kcpin, (char*)tun->buf, MNET_BUF_SIZE);
               if (ret > 0) {
                  proto_t pr;
                  if ( proto_probe(tun->buf, ret, &pr) ) {
                     if (pr.ptype == PROTO_TYPE_DATA) {
                        ret = mnet_chann_send(tun->tcpout, pr.u.data, pr.data_length);
                        if (ret < 0) {
                           cerr << "ikcp recv then fail to send: " << ret << endl;
                        }
                     } 
                     else if ((pr.ptype == PROTO_TYPE_CTRL) &&
                              (pr.u.cmd == PROTO_CMD_RESET))
                     {
                        cout << "reset tcp out" << endl;
                        ikcp_flush(tun->kcpin);
                        mnet_chann_close(tun->tcpout);
                        tun->tcpout = NULL;
                        break;
                     }
                  }
               }
            } while (ret > 0);
         }
      }
      else if (tun->tcpout == NULL &&
               current - tun->last_ti_try_connect > 5000)
      {
         tun->last_ti_try_connect = current;
         _tcpout_open_and_connect(tun);
      }

      mnet_poll( 100 );        // micro seconds
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
         const int mss = tun->kcpin->mss;
         long chann_ret = 0;
         do {
            int offset = proto_mark_data(tun->buf, 0);
            chann_ret = mnet_chann_recv(e->n, &tun->buf[offset], mss - offset);
            if (chann_ret > 0) {
               int kcp_ret = ikcp_send(tun->kcpin, (const char*)tun->buf, chann_ret + offset);
               if (kcp_ret < 0) {
                  cerr << "Fail to send kcp " << kcp_ret << endl;
               }
            }
         } while (chann_ret > 0);
         break;
      }

      case MNET_EVENT_ERROR: {
         cerr << "remote tcp error: " << e->err << endl;
         mnet_chann_close(e->n);
         tun->tcpout = NULL;
         break;
      }

      case MNET_EVENT_DISCONNECT:  {
         cout << "remote tcp disconnect !" << endl;
         mnet_chann_close(e->n);
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
         const int IKCP_OVERHEAD = 24;
         long ret = mnet_chann_recv(e->n, tun->buf, MNET_BUF_SIZE);
         if (ret >= IKCP_OVERHEAD) {
            proto_t pr;
            const unsigned char *data = &tun->buf[IKCP_OVERHEAD]; // IKCP_OVERHEAD

            if ( proto_probe(data, ret - IKCP_OVERHEAD, &pr) ) {
               if (pr.ptype == PROTO_TYPE_CTRL && pr.u.cmd == PROTO_CMD_CONNECT) {
                  cout << "udp connected !" << endl;
                  _remote_kcpin_destroy(tun);
                  _remote_kcpin_create(tun);
               }
            }
            
            ikcp_input(tun->kcpin, (const char*)tun->buf, ret);
         } else {
            cout << "udp from "
                 << mnet_chann_addr(e->n) << ":" << mnet_chann_port(e->n)
                 << " ret: " << ret << endl;
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
   if (tun && mnet_chann_state(tun->udpin) >= CHANN_STATE_CONNECTED) {
      return mnet_chann_send(tun->udpin, (void*)buf, len);
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
      }

      delete tun;
   }

   return 0;
}

#endif
