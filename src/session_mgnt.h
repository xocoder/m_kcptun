/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef SESSION_MGNT_H
#define SESSION_MGNT_H

#include "m_list.h"
#include "plat_net.h"

typedef struct {
   unsigned sid;
   chann_t *tcp;
   void *opaque;
   void *node;
} session_unit_t;

session_unit_t* session_create(lst_t *lst, unsigned sid, chann_t *tcp, void *opaque);

session_unit_t* session_find_sid(lst_t *lst, unsigned sid);

void session_destroy(lst_t *lst, session_unit_t *u);

#endif
