/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "session_proto.h"

int
proto_probe(const unsigned char *buf, int buf_len, proto_t *session) {
   if (buf && buf_len>0 && session) {
      session->ptype = buf[0];
      session->sid = (buf[1]<<8) | buf[2];
      if (session->ptype == PROTO_TYPE_CTRL) {
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
proto_mark_cmd(unsigned char *buf, unsigned sid, int cmd) {
   if (buf && cmd>0) {
      buf[0] = PROTO_TYPE_CTRL;
      buf[1] = (sid >> 8) & 0xff;
      buf[2] = sid & 0xff;
      buf[3] = cmd & 0xff;
      return 1;
   }
   return 0;
}

int
proto_mark_data(unsigned char *buf, unsigned sid) {
   if (buf) {
      buf[0] = PROTO_TYPE_DATA;
      buf[1] = (sid >> 8) & 0xff;
      buf[2] = sid & 0xff;
      return 3;
   }
   return -1;
}
