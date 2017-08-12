/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef SESSION_KCP_H
#define SESSION_KCP_H

/* session layout (bytes)
 *
 * | type (1) | session_id(2) | cmd (1) |
 *
 * | type (1) | session_id(2) | content (length) |
 *
 */

typedef enum {
   SESSION_TYPE_CTRL = 1,       /* session control */
   SESSION_TYPE_DATA            /* session data */
} SESSION_TYPE_t;

typedef enum {
   SESSION_CMD_CONNECT = 1,        /* session connect */
   SESSION_CMD_DISCONNECT,
   SESSION_CMD_RESET,
} SESSION_CMD_t;

typedef struct {
   int stype;
   int sid;
   union {
      int cmd;
      unsigned char *data;
   } u;
   int data_length;
} session_kcp_t;

int session_probe(const unsigned char *buf, int buf_len, session_kcp_t *session);

int session_mark_cmd(unsigned char *buf, int sid, int cmd); /* return offset */
int session_mark_data(unsigned char *buf, int sid); /* return offset */

#endif
