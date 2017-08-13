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
#include "crypto_kcp.h"

#include "session_proto.h"
#include "session_mgnt.h"

#include <stdio.h>
#include <string.h>
#include <iostream>

#ifdef LOCAL_KCP

using namespace std;

typedef struct {
   chann_t *udpout;
   ikcpcb *kcpout;
   unsigned kcp_op;

   lst_t *session_lst;
   unsigned session_idx;

   uint64_t ukey;
   uint64_t ti;
   uint64_t ti_last;

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
   ikcp_setmtu(tun->kcpout, tun->conf->mtu);

   if (tun->conf->fast == 3) {
      tun->kcpout->rx_minrto = 10;
   } else if (tun->conf->fast == 2) {
      tun->kcpout->rx_minrto = 40;
   }
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

      // tcp chann list
      tun->session_lst = lst_create();

      // crypto
      if (tun->conf->crypto) {
         tun->ukey = rc4_hash_key((const char*)tun->conf->key, strlen(tun->conf->key));
         tun->ti = mtime_current();
         tun->ti_last = tun->ti;
      }

      // tcp listen
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

      // udp
      if ( !_local_udpout_create(tun) ) {
         return 0;
      }
      
      // kcp
      if ( !_local_kcpout_create(tun) ) {
         return 0;
      }

      // when setup kcp, reset remote kcp
      {
         unsigned char buf[16] = {0};
         if ( proto_mark_cmd(buf, 0, PROTO_CMD_RESET) ) {
            ikcp_send(tun->kcpout, (const char*)buf, 16);
            tun->kcp_op = 0;
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
      while ( lst_count(tun->session_lst) ) {
         session_unit_t *u = (session_unit_t*)lst_first(tun->session_lst);
         mnet_chann_close(u->tcp);
         session_destroy(tun->session_lst, u->sid);
      }
      lst_destroy(tun->session_lst);

      _local_kcpout_destroy(tun);
      mnet_fini();
   }
   return 0;
}

static void
_local_network_runloop(tun_local_t *tun) {

   for (;;) {
      tun->ti = mtime_current();

      tun->kcp_op = 0;
      tun->ti_last = tun->ti;

      IUINT32 current = (IUINT32)(tun->ti / 1000);

      IUINT32 nextTime = ikcp_check(tun->kcpout, current);
      if (nextTime <= current) {
         ikcp_update(tun->kcpout, current);
      }


      if (ikcp_peeksize(tun->kcpout) > 0)
      {
         int ret = 0;

         do {
            ret = ikcp_recv(tun->kcpout, (char*)tun->buf, MNET_BUF_SIZE);
            if (ret > 0) {

               proto_t pr;
               if ( proto_probe(tun->buf, ret, &pr) )
               {
                  session_unit_t *u = session_find_sid(tun->session_lst, pr.sid);
                  if (u && pr.ptype == PROTO_TYPE_DATA)
                  {
                     int chann_ret = mnet_chann_send(u->tcp, pr.u.data, pr.data_length);
                     if (chann_ret < 0)
                     {
                        cerr << "ikcp recv then fail to send " << chann_ret << endl;
                     }
                  }
                  else if (u &&
                           pr.ptype == PROTO_TYPE_CTRL &&
                           pr.u.cmd == PROTO_CMD_CLOSE)
                  {
                     mnet_chann_close(u->tcp);
                     session_destroy(tun->session_lst, u->sid);
                     cout << "close tcp with sid " << pr.sid << endl;
                  }
               }
               else
               {
                  cerr << "ikcp recv invalid proto " << endl;
               }
            }
         } while (ret > 0);
      }

      tun->kcp_op += 1;

      mnet_poll( 1000 );        // micro seconds
   }
}

// 
// tcp in
void
_local_tcpin_listen(chann_event_t *e) {
   if (e->event == MNET_EVENT_ACCEPT) {

      tun_local_t *tun = (tun_local_t*)e->opaque;
      if ( tun ) {

         // setup local session
         unsigned sid = ++tun->session_idx;
         session_unit_t *u = session_create(tun->session_lst, sid, e->r, tun);
         if ( u ) {
            mnet_chann_set_cb(e->r, _local_tcpin_callback, u);

            // open remote tcp
            unsigned char buf[16] = { 0 };
            if ( proto_mark_cmd(buf, sid, PROTO_CMD_OPEN) ) {
               ikcp_send(tun->kcpout, (const char*)buf, 16);
               tun->kcp_op = 0;
            }

            cout << "accept tcpin: " << e->r << ", sid " << sid << endl;
            return;
         }
      }

      mnet_chann_disconnect(e->r);
   }
}

void
_local_tcpin_callback(chann_event_t *e) {
   session_unit_t *u = (session_unit_t*)e->opaque;
   tun_local_t *tun = (tun_local_t*)u->opaque;

   switch (e->event) {

      case MNET_EVENT_RECV: {
         const int mss = tun->kcpout->mss;
         long chann_ret = 0;
         do {
            int offset = proto_mark_data(tun->buf, u->sid);
            chann_ret = mnet_chann_recv(e->n, &tun->buf[offset], mss - offset);
            if (chann_ret > 0) {
               int kcp_ret = ikcp_send(tun->kcpout, (const char*)tun->buf, chann_ret + offset);
               if (kcp_ret < 0) {
                  cerr << "Fail to send kcp " << kcp_ret << endl;
               }
               tun->kcp_op = 0;
            }
         } while (chann_ret > 0);
         break;
      }

      case MNET_EVENT_ERROR: {
         cerr << "local tcp error: " << e->err << endl;
         break;
      }

      case MNET_EVENT_DISCONNECT:  {

         // send disconnect msg
         unsigned char buf[16] = { 0 };
         if ( proto_mark_cmd(buf, u->sid, PROTO_CMD_CLOSE) ) {
            ikcp_send(tun->kcpout, (const char*)buf, 16);
            tun->kcp_op = 0;
         }

         session_destroy(tun->session_lst, u->sid);
         mnet_chann_disconnect(e->n);

         cout << "local tcp error or disconnect !" << endl;
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
         if (ret > 0 && tun->conf->crypto) {
            ret = rc4_decrypt((const char*)tun->buf, ret, (char*)tun->buf, tun->ukey, (tun->ti>>20));
         }
         if (ret > 0) {
            ikcp_input(tun->kcpout, (const char*)tun->buf, ret);
            tun->kcp_op = 0;
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
      int ret = len;
      void *outbuf = (void*)buf;

      if (tun->conf->crypto) {
         ret = rc4_encrypt(buf, len, (char*)tun->buf, tun->ukey, (tun->ti>>20));
         outbuf = (void*)tun->buf;
      }

      if (mnet_chann_send(tun->udpout, outbuf, ret) == ret) {
         return len;
      }
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
