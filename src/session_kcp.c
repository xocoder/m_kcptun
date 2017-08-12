/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "session_kcp.h"

int
session_probe(const unsigned char *buf, int buf_len, session_kcp_t *session) {
   if (buf && buf_len>0 && session) {
      session->stype = buf[0];
      session->sid = (buf[1]<<8) | buf[2];
      if (session->stype == SESSION_TYPE_CTRL) {
         session->u.cmd = buf[3];
      } else {
         session->u.data = (unsigned char*)(buf + 3);

      }
      session->data_length = buf_len - 3;
      return 1;
   }
   return 0;
}

int
session_mark_cmd(unsigned char *buf, int sid, int cmd) {
   if (buf && cmd>0) {
      buf[0] = SESSION_TYPE_CTRL;
      buf[1] = (sid >> 8) & 0xff;
      buf[2] = sid & 0xff;
      buf[3] = cmd & 0xff;
      return 1;
   }
   return 0;
}

int
session_mark_data(unsigned char *buf, int sid) {
   if (buf) {
      buf[0] = SESSION_TYPE_DATA;
      buf[1] = (sid >> 8) & 0xff;
      buf[2] = sid & 0xff;
      return 3;
   }
   return -1;
}
