// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
//

#include "mnet_core.h"
#include "plat_time.h"

#include "ikcp.h"
#include "conf_kcp.h"
#include "m_rc4.h"
#include "m_xor64.h"

#include "session_proto.h"
#include "session_mgnt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

   int is_init;
   conf_kcp_t *conf;
   uint8_t buf[MKCP_BUF_SIZE];
} tun_local_t;


//
// tcp internal
static void _local_tcpin_listen(chann_msg_t *e);
static void _local_tcpin_callback(chann_msg_t *e);
static void _local_udpout_callback(chann_msg_t *e);
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
   if (mnet_chann_connect(tun->udpout, tun->conf->dest_ip, tun->conf->dest_port) <= 0) {
      cerr << "Fail to connect to udp out !" << endl;
      return 0;
   }

   mnet_chann_set_bufsize(tun->udpout, 512*1024);
   return 1;
}

static int
_local_kcpout_create(tun_local_t *tun) {
   srand(mtime_current());
   IUINT32 iconv = rand();
   cout << "using kcp_conv " << iconv  << endl;

   tun->kcpout = ikcp_create(iconv, tun);
   if (tun->kcpout == NULL) {
      cerr << "Fail to create kcp out !" << endl;
      return 0;
   }

   ikcp_setoutput(tun->kcpout, _local_kcpout_callback);
   ikcp_nodelay(tun->kcpout, tun->conf->nodelay, tun->conf->interval, tun->conf->resend, tun->conf->nc);
   ikcp_wndsize(tun->kcpout, tun->conf->snd_wndsize, tun->conf->rcv_wndsize);
   ikcp_setmtu(tun->kcpout, tun->conf->mtu);

   if (tun->conf->fast == 3) {
      tun->kcpout->rx_minrto = 20;
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
         tun->ukey = rc4_hash_key((const char*)tun->conf->key, 16);
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
      if (mnet_chann_listen(tcp, tun->conf->src_ip, tun->conf->src_port, 1) <= 0) {
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
            tun->kcp_op++;
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

   for (int i=0;;i++) {
      if (i >= 1024) {
         i=0; mtime_sleep(1);
      }

      tun->ti = mtime_current();

      if ((tun->ti - tun->ti_last) > 10000) {
         tun->ti_last = tun->ti;
         ikcp_update(tun->kcpout, tun->ti / 1000);
      }

      if (ikcp_peeksize(tun->kcpout) > 0) {
         int ret = 0;
         do {
            ret = ikcp_recv(tun->kcpout, (char*)tun->buf, MKCP_BUF_SIZE);
            if (ret > 0) {

               proto_t pr;
               if ( proto_probe(tun->buf, ret, &pr) )
               {
                  session_unit_t *u = session_find_sid(tun->session_lst, pr.sid);
                  if ( u ) {
                     if (pr.ptype == PROTO_TYPE_DATA)
                     {
                        int chann_ret = mnet_chann_send(u->tcp, pr.u.data, pr.data_length);
                        if (pr.u.data && pr.data_length>0 && chann_ret<0) {
                           cerr << "ikcp recv then fail to send " << chann_ret << endl;
                        }
                     }
                     else if (pr.ptype == PROTO_TYPE_CTRL) {
                        if (pr.u.cmd == PROTO_CMD_OPENED) {
                           u->connected = 1;
                           cout << "remote tcp connected" << endl;
                        }
                        else if (pr.u.cmd == PROTO_CMD_CLOSE)
                        {
                           mnet_chann_close(u->tcp);
                           session_destroy(tun->session_lst, u->sid);
                           cout << "close tcp with sid " << pr.sid << endl;
                        }
                     }
                  }
               }
               else
               {
                  cerr << "ikcp recv invalid proto " << endl;
               }
            }
         } while (ret > 0);
      }

      tun->kcp_op = 0;
      mnet_poll( 1000 ); // micro seconds
   }
}

// 
// tcp in
void
_local_tcpin_listen(chann_msg_t *e) {
   if (e->event == CHANN_EVENT_ACCEPT) {

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
               tun->kcp_op++;
            }

            cout << "accept tcpin: " << e->r << ", sid " << sid << endl;
            return;
         }
      }

      mnet_chann_disconnect(e->r);
   }
}

void
_local_tcpin_callback(chann_msg_t *e) {
   session_unit_t *u = (session_unit_t*)e->opaque;
   tun_local_t *tun = (tun_local_t*)u->opaque;

   switch (e->event) {

      case CHANN_EVENT_RECV: {
         if ( u->connected ) {
            const int mss = tun->kcpout->mss - MKCP_OVERHEAD;
            long chann_ret = 0;
            do {
               int offset = proto_mark_data(tun->buf, u->sid);
               chann_ret = mnet_chann_recv(e->n, &tun->buf[offset], mss - offset);
               if (chann_ret > 0) {
                  int kcp_ret = ikcp_send(tun->kcpout, (const char*)tun->buf, chann_ret + offset);
                  if (kcp_ret < 0) {
                     cerr << "Fail to send kcp " << kcp_ret << endl;
                  }
                  tun->kcp_op++;
               }
            } while (chann_ret > 0);
         }
         break;
      }

      case CHANN_EVENT_DISCONNECT:  {

         // send disconnect msg
         unsigned char buf[16] = { 0 };
         if ( proto_mark_cmd(buf, u->sid, PROTO_CMD_CLOSE) ) {
            ikcp_send(tun->kcpout, (const char*)buf, 16);
            tun->kcp_op++;
         }

         session_destroy(tun->session_lst, u->sid);
         mnet_chann_close(e->n);

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
_local_udpout_callback(chann_msg_t *e) {
   tun_local_t *tun = (tun_local_t*)e->opaque;   

   switch (e->event) {

      case CHANN_EVENT_RECV: {
         long ret = mnet_chann_recv(e->n, tun->buf, MKCP_BUF_SIZE);
         int data_len = ret - XOR64_CHECKSUM_SIZE;
         uint8_t *data = &tun->buf[XOR64_CHECKSUM_SIZE];

         if (data_len >= MKCP_OVERHEAD &&
             xor64_checksum_check(data, data_len, tun->buf))
         {

            if ( tun->conf->crypto ) {
               data_len = rc4_decrypt((const char*)data, data_len,
                                      (char*)tun->buf, MKCP_BUF_SIZE,
                                      tun->ukey, (tun->ti>>20));
               data = tun->buf;
            }

            if (data_len >= MKCP_OVERHEAD) {
               ikcp_input(tun->kcpout, (const char*)data, data_len);
               tun->kcp_op++;
            }
         }
         break;
      }

      case CHANN_EVENT_DISCONNECT:  {
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
      int data_len = len;
      uint8_t *data = &tun->buf[XOR64_CHECKSUM_SIZE];

      if ( tun->conf->crypto ) {
         data_len = rc4_encrypt(buf, len,
                                (char*)data, MKCP_BUF_SIZE - XOR64_CHECKSUM_SIZE,
                                tun->ukey, (tun->ti>>20));
      } else {
         memcpy(data, buf, len);
      }

      if ( xor64_checksum_gen((uint8_t*)data, data_len, tun->buf) ) {
         int ret = mnet_chann_send(tun->udpout, tun->buf, data_len + XOR64_CHECKSUM_SIZE);
         if (ret == (data_len + XOR64_CHECKSUM_SIZE)) {
            return len;
         }
      }
   }
   return 0;
}

static tun_local_t *g_tun;

static void
hook_aexit(void) {
   if (g_tun) {
      _local_network_fini(g_tun);
   }
}

int
main(int argc, const char *argv[]) {

   atexit(hook_aexit);

   tun_local_t *tun = new tun_local_t;
   memset(tun, 0, sizeof(*tun));

   if ( tun ) {

      tun->conf = conf_create(argc, argv);
      if ( tun->conf ) {

         if ( _local_network_init(tun) ) {
            g_tun = tun;
            
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
