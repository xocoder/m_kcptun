/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef SESSION_MGNT_H
#define SESSION_MGNT_H

#include "mnet_core.h"
#include "m_skiplist.h"

typedef struct {
   unsigned sid;                /* session id */
   chann_t *tcp;                /* tcp in */
   int connected;               /* remote tcp state */
   void *opaque;                /* user info */
} session_unit_t;

session_unit_t* session_create(skt_t *lst, unsigned sid, chann_t *tcp, void *opaque);

session_unit_t* session_find_sid(skt_t *lst, unsigned sid);

void session_destroy(skt_t *lst, session_unit_t *u);

#endif
