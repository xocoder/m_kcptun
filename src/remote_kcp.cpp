// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
//

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <iostream>

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

#ifdef REMOTE_KCP

using namespace std;

typedef struct {
   chann_t *udpin;              // listen

   ikcpcb *kcpin;
   uint32_t kcpconv;            // kcp conv

   skt_t *session_lst;          // session unit list

   uint64_t ukey;               // secret
   uint64_t ti;                 // current time

   int is_init;
   conf_kcp_t *conf;            // conf handle

   tmr_t *tmr;                  // timer list
   tmr_timer_t *tm;
   
   rskcp_t *rt;
   uint8_t buf[MKCP_BUF_SIZE];
} tun_remote_t;


//
// tcp internal
static void _remote_tcpout_callback(chann_msg_t *e);
static void _remote_udpin_callback(chann_msg_t *e);
static int _remote_kcpin_callback(const char *buf, int len, ikcpcb *kcp, void *user);

static int
_tcpout_open_and_connect(tun_remote_t *tun, unsigned sid) {
   chann_t *tcp = mnet_chann_open(CHANN_TYPE_STREAM);
   if (tcp == NULL) {
      cerr << "Fail to create tcp out !" << endl;
      return 0;
   }

   session_unit_t *u = session_create(tun->session_lst, sid, tcp, tun);
   if ( u ) {
      mnet_chann_set_cb(tcp, _remote_tcpout_callback, u);
      if (mnet_chann_connect(tcp, tun->conf->dest_ip, tun->conf->dest_port) > 0) {
         cout << "tcp out try connect with sid " << sid << endl;
         return 1;
      }
   } else {
      mnet_chann_close(tcp);
   }

   cerr << "tcp out fail to connect with sid " << sid << endl;
   return 0;
}



// 
// udp & kcp
static int
_remote_kcpin_create(tun_remote_t *tun) {
   tun->kcpin = ikcp_create(tun->kcpconv, tun);
   if (tun->kcpin == NULL) {
      cerr << "Fail to create kcp in !" << endl;
      return 0;
   }

   ikcp_setoutput(tun->kcpin, _remote_kcpin_callback);
   ikcp_nodelay(tun->kcpin, tun->conf->nodelay, tun->conf->interval, tun->conf->resend, tun->conf->nc);
   ikcp_wndsize(tun->kcpin, tun->conf->snd_wndsize, tun->conf->rcv_wndsize);
   ikcp_setmtu(tun->kcpin, tun->conf->mtu);

   if (tun->conf->fast == 3) {
      tun->kcpin->rx_minrto = 20;
   } else if (tun->conf->fast == 2) {
      tun->kcpin->rx_minrto = 40;
   }
   return 1;
}

static void
_remote_kcpin_destroy(tun_remote_t *tun) {
   if ( tun->kcpin ) {
      ikcp_release(tun->kcpin);
      tun->kcpin = NULL;
   }
}

static void
_remote_kcpin_reset(tun_remote_t *tun, uint32_t kcpconv) {
   
   cout << "udp & kcp reset " << kcpconv << endl;
   tun->kcpconv = kcpconv;

   _remote_kcpin_destroy(tun);
   _remote_kcpin_create(tun);

   while (skt_count(tun->session_lst) > 0) {
      session_unit_t *u = (session_unit_t*)skt_popf(tun->session_lst);
      mnet_chann_close(u->tcp);
      session_destroy(NULL, u);
   }
   skt_destroy(tun->session_lst);
   tun->session_lst = skt_create();
}

static void
_remote_tmr_callback(tmr_timer_t *tm, void *opaque) {
   tun_remote_t *tun = (tun_remote_t*)opaque;
   ikcp_update(tun->kcpin, tun->ti / 1000);   
}

// 
// initial
static int
_remote_network_init(tun_remote_t *tun) {
   if (tun && !tun->is_init) {
      mnet_init();

      // tcp list
      tun->session_lst = skt_create();

      // crytpo
      if (tun->conf->crypto) {
         tun->ukey = rc4_hash_key((const char*)tun->conf->key, strlen(tun->conf->key));
         tun->ti = mtime_current();
      }

      // udp
      tun->udpin = mnet_chann_open(CHANN_TYPE_DGRAM);
      if (tun->udpin == NULL) {
         cerr << "Fail to create udp in !" << endl;
         return 0;
      }

      mnet_chann_set_cb(tun->udpin, _remote_udpin_callback, tun);
      if (mnet_chann_listen(tun->udpin, tun->conf->src_ip, tun->conf->src_port, 1) <= 0) {
         cerr << "Fail to connect to udp in !" << endl;
         return 0;
      }
      mnet_chann_set_bufsize(tun->udpin, 4096*1024);

      // kcp
      if ( !_remote_kcpin_create(tun) ) {
         return 0;
      }

      // setup timer
      tun->tmr = tmr_create_lst();
      tun->tm = tmr_add(tun->tmr, tun->ti, 10000, 1, tun, _remote_tmr_callback);

      tun->rt = rskcp_create();
      
      tun->is_init = 1;
      return 1;
   }
   return 0;
}

static int
_remote_network_fini(tun_remote_t *tun) {
   if (tun) {
      tmr_destroy_lst(tun->tmr);      
      while (skt_count(tun->session_lst) > 0) {
         session_unit_t *u = (session_unit_t*)skt_popf(tun->session_lst);
         mnet_chann_close(u->tcp);      
         session_destroy(NULL, u);
      }
      skt_destroy(tun->session_lst);      
      _remote_kcpin_destroy(tun);
      rskcp_release(tun->rt);
      mnet_fini();
      return 1;
   }
   return 0;
}


static void
_remote_network_runloop(tun_remote_t *tun) {

   for (int i=0;;i++) {
      if (i >= 4096) {
         i=0; mtime_sleep(1);
      }

      tun->ti = mtime_current();
      tmr_update_lst(tun->tmr, tun->ti);


      if (ikcp_peeksize(tun->kcpin) > 0) {
         int ret = 0;
         do {
            ret = ikcp_recv(tun->kcpin, (char*)tun->buf, MKCP_BUF_SIZE);
            if (ret > 0) {

               proto_t pr;
               if ( proto_probe(tun->buf, ret, &pr) )
               {
                  session_unit_t *u = session_find_sid(tun->session_lst, pr.sid);
                  if (u &&
                      pr.ptype == PROTO_TYPE_DATA &&
                      mnet_chann_state(u->tcp) == CHANN_STATE_CONNECTED)
                  {
                     int chann_ret = mnet_chann_send(u->tcp, pr.u.data, pr.data_length);
                     if (chann_ret < 0) {
                        cerr << "ikcp recv then fail to send: " << chann_ret << endl;
                     }
                  }
                  else if (u &&
                           pr.ptype == PROTO_TYPE_CTRL &&
                           pr.u.cmd == PROTO_CMD_CLOSE)
                  {
                     mnet_chann_close(u->tcp);
                     // destroy session in tcp callback
                  }
                  else if (pr.ptype == PROTO_TYPE_CTRL &&
                           pr.u.cmd == PROTO_CMD_OPEN)
                  {
                     _tcpout_open_and_connect(tun, pr.sid);
                  }
                  else if (pr.u.cmd != PROTO_CMD_RESET) {
                     cerr << "invalid proto cmd" << endl;
                  }
               }
               else {
                  cerr << "ikcp recv invalid proto " << endl;
                  break;
               }
            }
         } while (ret > 0);
      }

      mnet_poll( 1000 );        // micro seconds
   }
}

// 
// tcp out
static void
_remote_tcpout_callback(chann_msg_t *e) {
   session_unit_t *u = (session_unit_t*)e->opaque;
   tun_remote_t *tun = (tun_remote_t*)u->opaque;

   switch (e->event) {
      
      case CHANN_EVENT_CONNECTED: {
         if ( !u->connected ) {
            u->connected = 1;
            int offset = proto_mark_cmd(tun->buf, u->sid, PROTO_CMD_OPENED);
            int kcp_ret = ikcp_send(tun->kcpin, (const char*)tun->buf, offset);
            if (kcp_ret < 0) {
               cerr << "Fail to send kcp connected state" << endl;
            }
            cout << "Tcp out connected." << endl;
         }
         break;
      }

      case CHANN_EVENT_RECV: {
         if (mnet_chann_state(e->n) == CHANN_STATE_CONNECTED) {
            const int mss = tun->kcpin->mss;
            long chann_ret = 0;
            do {
               int offset = proto_mark_data(tun->buf, u->sid);
               chann_ret = mnet_chann_recv(e->n, &tun->buf[offset], mss - offset);
               if (chann_ret > 0) {
                  int kcp_ret = ikcp_send(tun->kcpin, (const char*)tun->buf, chann_ret + offset);
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
         unsigned char buf[16] = { 0 };
         if ( proto_mark_cmd(buf, u->sid, PROTO_CMD_CLOSE) ) {
            ikcp_send(tun->kcpin, (const char*)buf, 16);
         }

         mnet_chann_close(e->n);
         session_destroy(tun->session_lst, u);

         cout << "remote tcp disconnect, remain " << skt_count(tun->session_lst) << endl;
         break;
      }

      default: {
         break;
      }
   }
}

// 
// udp out
static void
_remote_udpin_callback(chann_msg_t *e) {
   tun_remote_t *tun = (tun_remote_t*)e->opaque;   

   switch (e->event) {

      case CHANN_EVENT_RECV: {
         long ret = mnet_chann_recv(e->n, tun->buf, MKCP_BUF_SIZE);

         if (ret > MKCP_OVERHEAD) {
            const int rs_offset = tun->conf->rs ? sizeof(uint16_t) : 0;
            uint8_t *data = tun->buf + rs_offset;
            int data_len = tun->conf->rs ? rskcp_dec_info(tun->rt, ret) : (ret - XOR64_CHECKSUM_SIZE);

            if (tun->conf->rs) {
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
                  data = tun->buf;
               }

               if (data_len >= MKCP_OVERHEAD) {
                  proto_t pr;
                  const uint8_t *proto_data = &data[MKCP_OVERHEAD];

                  if (proto_probe(proto_data, data_len - MKCP_OVERHEAD, &pr) &&
                      pr.ptype == PROTO_TYPE_CTRL &&
                      pr.u.cmd == PROTO_CMD_RESET &&
                      tun->kcpconv != ikcp_getconv(data))
                  {
                     _remote_kcpin_reset(tun, ikcp_getconv(data));
                  }

                  ikcp_input(tun->kcpin, (const char*)data, data_len);
                  break;
               }
            }
         }

         chann_addr_t addr;
         mnet_chann_addr(e->n, &addr);
         cout << "invalid udp packet from "
              << addr.ip << ":" << addr.port
              << " ret: " << ret << endl;
         break;
      }

      case CHANN_EVENT_DISCONNECT:  {
         cout << "remote udp disconnect !" << endl;
         mnet_chann_close(e->n);
         break;
      }

      default: {
         break;
      }
   }
}

//
// kcp out
static int
_remote_kcpin_callback(const char *buf, int len, ikcpcb *kcp, void *user) {
   tun_remote_t *tun = (tun_remote_t*)user;

   if (tun && mnet_chann_state(tun->udpin) >= CHANN_STATE_CONNECTED) {
      const int rs_offset = tun->conf->rs ? sizeof(uint16_t) : 0;
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

      if ( tun->conf->rs ) {
         int plen = 0;
         *((uint16_t*)tun->buf) = data_len;
         int raw_len = rskcp_enc_info(tun->rt, (data_len + rs_offset), &plen);
         ret = rskcp_encode(tun->rt, tun->buf, (data_len + rs_offset), &tun->buf[raw_len]);
         data_len = raw_len + plen;
      } else {
         ret = xor64_checksum_gen((uint8_t*)data, data_len, &data[data_len]);
         data_len += XOR64_CHECKSUM_SIZE;
      }

      if ( ret ) {
         ret = mnet_chann_send(tun->udpin, tun->buf, data_len);
         if (ret == data_len) {
            return len;
         }
      }
   }
   return 0;
}

int
main(int argc, const char *argv[]) {

   tun_remote_t *tun = new tun_remote_t;
   memset(tun, 0, sizeof(*tun));

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
