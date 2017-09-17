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
#include <string.h>
#include <iostream>

#ifdef REMOTE_KCP

using namespace std;

typedef struct {
   chann_t *udpin;              // listen

   ikcpcb *kcpin;
   uint32_t kcpconv;            // kcp conv

   unsigned kcp_op;             // for kcp update calc

   lst_t *session_lst;          // session unit list

   uint64_t ukey;               // secret
   uint64_t ti;
   uint64_t ti_last;

   int is_init;
   conf_kcp_t *conf;            // conf handle
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

static void
_remote_tcp_close(tun_remote_t *tun) {
   while ( lst_count(tun->session_lst) ) {
      session_unit_t *u = (session_unit_t*)lst_first(tun->session_lst);
      mnet_chann_close(u->tcp);
      session_destroy(tun->session_lst, u->sid);
   }
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



// 
// initial
static int
_remote_network_init(tun_remote_t *tun) {
   if (tun && !tun->is_init) {
      mnet_init();

      // tcp list
      tun->session_lst = lst_create();

      // crytpo
      if (tun->conf->crypto) {
         tun->ukey = rc4_hash_key(tun->conf->key, strlen(tun->conf->key));
         tun->ti = mtime_current();
         tun->ti_last = tun->ti;
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
      
      tun->is_init = 1;
      return 1;
   }
   return 0;
}

static int
_remote_network_fini(tun_remote_t *tun) {
   if (tun) {
      _remote_tcp_close(tun);
      lst_destroy(tun->session_lst);
      _remote_kcpin_destroy(tun);
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

      if ((tun->ti - tun->ti_last) > 10000) {
         tun->ti_last = tun->ti;
         ikcp_update(tun->kcpin, tun->ti / 1000);
      }


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

      tun->kcp_op = 0;
      mnet_poll( 1000 );        // micro seconds
   }
}

// 
// tcp out
void
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
            tun->kcp_op++;
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
            ikcp_send(tun->kcpin, (const char*)buf, 16);
            tun->kcp_op++;
         }

         mnet_chann_close(e->n);
         session_destroy(tun->session_lst, u->sid);

         cout << "remote tcp disconnect, remain " << lst_count(tun->session_lst) << endl;
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
_remote_udpin_callback(chann_msg_t *e) {
   tun_remote_t *tun = (tun_remote_t*)e->opaque;   

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

               proto_t pr;
               const uint8_t *proto_data = &data[MKCP_OVERHEAD];

               if (proto_probe(proto_data, data_len - MKCP_OVERHEAD, &pr) &&
                   pr.ptype == PROTO_TYPE_CTRL &&
                   pr.u.cmd == PROTO_CMD_RESET &&
                   tun->kcpconv != ikcp_getconv(data))
               {
                  tun->kcpconv = ikcp_getconv(data);
                  cout << "udp & kcp reset " << tun->kcpconv << endl;
                  _remote_kcpin_destroy(tun);
                  _remote_kcpin_create(tun);
                  _remote_tcp_close(tun);
               }

               ikcp_input(tun->kcpin, (const char*)data, data_len);
               tun->kcp_op++;
               break;
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
int
_remote_kcpin_callback(const char *buf, int len, ikcpcb *kcp, void *user) {
   tun_remote_t *tun = (tun_remote_t*)user;

   if (tun && mnet_chann_state(tun->udpin) >= CHANN_STATE_CONNECTED) {
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
         int ret = mnet_chann_send(tun->udpin, tun->buf, data_len + XOR64_CHECKSUM_SIZE);
         if (ret == (data_len + XOR64_CHECKSUM_SIZE)) {
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
