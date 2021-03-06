// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>

#include "plat_os.h"
#include "plat_time.h"
#include "mnet_core.h"

#include "ikcp.h"
#include "conf_kcp.h"
#include "m_rc4.h"
#include "m_xor64.h"
#include "m_timer.h"

#include "rs_kcp.h"
#include "session_proto.h"
#include "session_mgnt.h"

#if !defined(PLAT_OS_WIN)
#include <signal.h>
#endif

#ifdef KCP_LOCAL

using namespace std;

typedef struct {
   int udpout_idx;
   chann_t *udpout[IP_COUNT];   // UDP output
   ikcpcb *kcpout;              // KCP output

   skt_t *session_lst;          // session
   unsigned session_idx;

   uint64_t ukey;               // 
   uint64_t ti;                 // time (micro second)

   int is_init;
   conf_kcp_t *conf;

   tmr_t *tmr;                  // timer list
   tmr_timer_t *tm;

   rskcp_t *rt;
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
static inline uint32_t
_local_gen_kcpconv(void) {
   srand(mtime_current());
   return rand();
}

static int
_local_udpout_create(tun_local_t *tun) {
   for (int i=0; i<tun->conf->src_count; i++) {
      tun->udpout[i] = mnet_chann_open(CHANN_TYPE_DGRAM);
      if (tun->udpout[i] == NULL) {
         cerr << "Fail to create udp out !" << endl;
         return 0;
      }

      mnet_chann_set_cb(tun->udpout[i], _local_udpout_callback, tun);
      if (mnet_chann_connect(tun->udpout[i], tun->conf->dest_ip[i], tun->conf->dest_port[i]) <= 0) {
         cerr << "Fail to connect to udp out !" << endl;
         return 0;
      }

      mnet_chann_set_bufsize(tun->udpout[i], 512*1024);
   }
   return 1;
}

static int
_local_kcpout_create(tun_local_t *tun) {
   ikcp_allocator(kcp_malloc, kcp_free);

   IUINT32 iconv = _local_gen_kcpconv();
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


static void
_local_tmr_callback(tmr_timer_t *tm, void *opaque) {
   tun_local_t *tun = (tun_local_t*)opaque;
   ikcp_update(tun->kcpout, tun->ti / 1000);   
}

// 
// initial
static int
_local_network_init(tun_local_t *tun) {
   if (tun && !tun->is_init) {
      mnet_init();

      // tcp chann list
      tun->session_lst = skt_create();

      // crypto
      if (tun->conf->crypto) {
         tun->ukey = rc4_hash_key((const char*)tun->conf->key, strlen(tun->conf->key));
         tun->ti = mtime_current();
      }

      // tcp listen
      chann_t *tcp = mnet_chann_open(CHANN_TYPE_STREAM);
      if (tcp == NULL) {
         cerr << "Fail to create listen tcp !" << endl;
         return 0;
      }

      mnet_chann_set_cb(tcp, _local_tcpin_listen, tun);
      if (mnet_chann_listen(tcp, tun->conf->src_ip[0], tun->conf->src_port[0], 1) <= 0) {
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
         }
      }

      // setup timer
      tun->tmr = tmr_create_lst();
      tun->tm = tmr_add(tun->tmr, tun->ti, 10000, 1, tun, _local_tmr_callback);

      if (tun->conf->rs_data) {
         tun->rt = rskcp_create(tun->conf->rs_data, tun->conf->rs_parity);
      }

      tun->is_init = 1;
      return 1;
   }
   return 0;
}

static int
_local_network_fini(tun_local_t *tun) {
   if (tun && tun->is_init) {
      tmr_destroy_lst(tun->tmr);
      while (skt_count(tun->session_lst) > 0) {
         session_unit_t *u = (session_unit_t*)skt_first(tun->session_lst);
         mnet_chann_close(u->tcp);
         session_destroy(NULL, u);
      }
      skt_destroy(tun->session_lst);
      _local_kcpout_destroy(tun);
      rskcp_release(tun->rt);
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
      tmr_update_lst(tun->tmr, tun->ti);


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
                           session_destroy(tun->session_lst, u);
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
      
      mnet_poll( 1000 ); // micro seconds
   }
}

// 
// tcp in
static inline int
_local_kcp_can_send_more(tun_local_t *tun) {
   return (ikcp_waitsnd(tun->kcpout) < (3*tun->conf->snd_wndsize));
}

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

               if (chann_ret > 0 && _local_kcp_can_send_more(tun)) {
                  int kcp_ret = ikcp_send(tun->kcpout, (const char*)tun->buf, chann_ret + offset);
                  if (kcp_ret < 0) {
                     cerr << "Fail to send kcp " << kcp_ret << endl;
                  }
               }
            } while (chann_ret > 0);
         }
         break;
      }

      case CHANN_EVENT_DISCONNECT:  {

         // send disconnect msg
         if ( _local_kcp_can_send_more(tun) ) {
            unsigned char buf[16] = { 0 };
            if ( proto_mark_cmd(buf, u->sid, PROTO_CMD_CLOSE) ) {
               ikcp_send(tun->kcpout, (const char*)buf, 16);
            }
         }

         session_destroy(tun->session_lst, u);
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

         if (ret > MKCP_OVERHEAD) {
            const int rs_offset = tun->rt ? sizeof(uint16_t) : 0;
            uint8_t *data = tun->buf + rs_offset;
            int data_len = tun->rt ? rskcp_dec_info(tun->rt, ret) : (ret - XOR64_CHECKSUM_SIZE);
            
            if ( tun->rt ) {
               ret = rskcp_decode(tun->rt, tun->buf, ret, &tun->buf[data_len]);
               data_len = *((uint16_t*)tun->buf); // restore data_len
            } else {
               ret = xor64_checksum_check(data, data_len, &tun->buf[data_len]);
            }

            if ( ret ) {
               if ( tun->conf->crypto ) {
                  data_len = rc4_decrypt((const char*)data, data_len,
                                         (char*)tun->buf, MKCP_BUF_SIZE,
                                         tun->ukey, (tun->ti>>20));
                  if (data_len < 0) {
                     return;
                  }
                  data = tun->buf;
               }

               if (data_len >= MKCP_OVERHEAD) {
                  ikcp_input(tun->kcpout, (const char*)data, data_len);
               }
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
   chann_t *udpout = tun->udpout[tun->udpout_idx];
   tun->udpout_idx = (tun->udpout_idx + 1) % tun->conf->src_count;

   if (tun && mnet_chann_state(udpout) >= CHANN_STATE_CONNECTED) {
      const int rs_offset = tun->rt ? sizeof(uint16_t) : 0;
      int data_len = len;
      uint8_t *data = tun->buf + rs_offset;

      if ( tun->conf->crypto ) {
         data_len = rc4_encrypt(buf, len,
                                (char*)data, MKCP_BUF_SIZE - XOR64_CHECKSUM_SIZE,
                                tun->ukey, (tun->ti>>20));
      } else {
         memcpy(data, buf, len);
      }

      int ret = 0;
      
      if ( tun->rt ) {
         int plen = 0;
         *((uint16_t*)tun->buf) = data_len;
         int raw_len = rskcp_enc_info(tun->rt, (data_len + rs_offset), &plen);
         ret = rskcp_encode(tun->rt, tun->buf, (data_len + rs_offset), &tun->buf[raw_len]);
         data_len = raw_len + plen;
      } else {
         ret = xor64_checksum_gen((uint8_t*)data, data_len, &tun->buf[data_len]);
         data_len += XOR64_CHECKSUM_SIZE;
      }

      if (ret && data_len>0) {
         ret = mnet_chann_send(udpout, tun->buf, data_len);
         if (ret == data_len) {
            return len;
         }
      }
   }
   return 0;
}

int
main(int argc, const char *argv[]) {

   tun_local_t *tun = new tun_local_t;
   memset(tun, 0, sizeof(*tun));

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
