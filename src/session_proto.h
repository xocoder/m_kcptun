/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef SESSION_PROTO_H
#define SESSION_PROTO_H

/* session protocol (bytes)
 *
 * | type (1) | session_id(2) | cmd (1) |
 *
 * | type (1) | session_id(2) | content (length) |
 *
 */

typedef enum {
   PROTO_TYPE_CTRL = 1,         /* control unit */
   PROTO_TYPE_DATA              /* data unit */
} PROTO_TYPE_t;

typedef enum {
   PROTO_CMD_OPEN = 1,          /* tcp connect */
   PROTO_CMD_OPENED,            /* tcp connected, remote to local */
   PROTO_CMD_CLOSE,             /* tcp disconnect */
   PROTO_CMD_RESET,             /* udp reconnect */
} PROTO_CMD_t;

typedef struct {
   int ptype;
   unsigned sid;                /* session id */
   union {
      int cmd;
      unsigned char *data;
   } u;
   int data_length;
} proto_t;

int proto_probe(const unsigned char *buf, int buf_len, proto_t *session);

int proto_mark_cmd(unsigned char *buf, unsigned sid, int cmd); /* return offset */
int proto_mark_data(unsigned char *buf, unsigned sid); /* return offset */

#endif
