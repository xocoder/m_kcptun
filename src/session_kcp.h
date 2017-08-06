/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef SESSION_KCP_H
#define SESSION_KCP_H

/* packet layout
 */

typedef enum {
   PACKET_CMD_CTRL = 1,         /* control packet */
   PACKET_CMD_DATA              /* data packet */
} KCP_PAKCET_t;

typedef enum {
   CTRL_CMD_CONNECT = 1,        /* tcp connect */
   CTRL_CMD_DISCONNECT,         /* tcp disconnect */
} CTRL_CMD_t;

#endif
