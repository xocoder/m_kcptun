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
   unsigned char buf[MNET_BUF_SIZE];
} tun_remote_t;


//
// tcp internal
static void _remote_tcpout_callback(chann_event_t *e);
static void _remote_udpin_callback(chann_event_t *e);
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
      tun->kcpin->rx_minrto = 10;
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
      if (mnet_chann_listen_ex(tun->udpin, tun->conf->src_ip, tun->conf->src_port, 1) <= 0) {
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

static void
_remote_tcp_reset(tun_remote_t *tun) {
   while ( lst_count(tun->session_lst) ) {
      session_unit_t *u = (session_unit_t*)lst_first(tun->session_lst);
      mnet_chann_close(u->tcp);
   }
}

static int
_remote_network_fini(tun_remote_t *tun) {
   if (tun) {
      _remote_tcp_reset(tun);
      lst_destroy(tun->session_lst);

      _remote_kcpin_destroy(tun);
      mnet_fini();
      return 1;
   }
   return 0;
}


static void
_remote_network_runloop(tun_remote_t *tun) {

   for (;;) {
      tun->ti = mtime_current();

      if (tun->kcp_op>0 && (tun->ti - tun->ti_last)>1000) {
         tun->ti_last = tun->ti;

         IUINT32 current = (IUINT32)(tun->ti / 1000);

         IUINT32 nextTime = ikcp_check(tun->kcpin, current);
         if (nextTime <= current + 10) {
            ikcp_update(tun->kcpin, current);
         }
      }


      if (ikcp_peeksize(tun->kcpin) > 0)
      {
         int ret = 0;

         do {
            ret = ikcp_recv(tun->kcpin, (char*)tun->buf, MNET_BUF_SIZE);
            if (ret > 0) {

               proto_t pr;
               if ( proto_probe(tun->buf, ret, &pr) )
               {
                  session_unit_t *u = session_find_sid(tun->session_lst, pr.sid);
                  if (u &&
                      pr.ptype == PROTO_TYPE_DATA)
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

      tun->kcp_op += 1;

      mnet_poll( 10 );        // micro seconds
   }
}

// 
// tcp out
void
_remote_tcpout_callback(chann_event_t *e) {
   session_unit_t *u = (session_unit_t*)e->opaque;
   tun_remote_t *tun = (tun_remote_t*)u->opaque;

   switch (e->event) {
      
      case MNET_EVENT_CONNECTED: {
         if ( !u->connected ) {
            u->connected = 1;
            int offset = proto_mark_cmd(tun->buf, u->sid, PROTO_CMD_OPENED);
            int kcp_ret = ikcp_send(tun->kcpin, (const char*)tun->buf, offset);
            if (kcp_ret < 0) {
               cerr << "Fail to send kcp connected state" << endl;
            }
            tun->kcp_op = 0;
            cout << "Tcp out connected !" << endl;
         }
         break;
      }

      case MNET_EVENT_RECV: {
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
                  tun->kcp_op = 0;
               }
            } while (chann_ret > 0);
         }
         break;
      }

      case MNET_EVENT_ERROR: {
         if (mnet_chann_state(e->n) < CHANN_STATE_CONNECTED) {
            cerr << "remote tcp to connect: " << e->err << endl;
         } else {
            cerr << "remote tcp error: " << e->err << endl;
         }
         break;
      }

      case MNET_EVENT_DISCONNECT:  {

         // send disconnect msg
         unsigned char buf[16] = { 0 };
         if ( proto_mark_cmd(buf, u->sid, PROTO_CMD_CLOSE) ) {
            ikcp_send(tun->kcpin, (const char*)buf, 16);
            tun->kcp_op = 0;
         }

         session_destroy(tun->session_lst, u->sid);
         mnet_chann_close(e->n);

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
_remote_udpin_callback(chann_event_t *e) {
   tun_remote_t *tun = (tun_remote_t*)e->opaque;   

   switch (e->event) {

      case MNET_EVENT_RECV: {
         const int IKCP_OVERHEAD = 24; // kcp header

         long ret = mnet_chann_recv(e->n, tun->buf, MNET_BUF_SIZE);

         if (ret>IKCP_OVERHEAD && tun->conf->crypto) {
            ret = rc4_decrypt((const char*)tun->buf, ret, (char*)tun->buf, tun->ukey, (tun->ti>>20));
         }

         if (ret >= IKCP_OVERHEAD) {

            proto_t pr;
            const unsigned char *data = &tun->buf[IKCP_OVERHEAD]; // IKCP_OVERHEAD

            if (proto_probe(data, ret - IKCP_OVERHEAD, &pr) &&
                pr.ptype == PROTO_TYPE_CTRL &&
                pr.u.cmd == PROTO_CMD_RESET &&
                tun->kcpconv != ikcp_getconv(tun->buf))
            {
                  cout << "udp & kcp reset" << endl;
                  tun->kcpconv = ikcp_getconv(tun->buf);
                  _remote_kcpin_destroy(tun);
                  _remote_kcpin_create(tun);
                  _remote_tcp_reset(tun);
            }

            ikcp_input(tun->kcpin, (const char*)tun->buf, ret);
            tun->kcp_op = 0;
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
      int ret = len;
      void *outbuf = (void*)buf;

      if (tun->conf->crypto) {
         ret = rc4_encrypt(buf, len, (char*)tun->buf, tun->ukey, (tun->ti>>20));
         outbuf = (void*)tun->buf;
      }

      if (mnet_chann_send(tun->udpin, outbuf, ret) == ret) {
         return len;
      }
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
