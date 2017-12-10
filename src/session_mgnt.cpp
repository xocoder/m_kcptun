/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "session_mgnt.h"
#include <stdlib.h>
#include <string.h>

session_unit_t*
session_create(skt_t *lst, unsigned sid, chann_t *tcp, void *opaque) {
   if (lst && tcp) {
      session_unit_t *u = (session_unit_t*)malloc(sizeof(*u));
      if (u) {
         u->sid = sid;
         u->tcp = tcp;
         u->opaque = opaque;
         u->connected = 0;
         if ( skt_insert(lst, sid, u) ) {
            return u;
         }
         free(u); // fail to insert
      }
   }
   return NULL;
}

session_unit_t*
session_find_sid(skt_t *lst, unsigned sid) {
   session_unit_t *u = (session_unit_t*)skt_query(lst, sid);
   if (u && u->sid == sid) {
      return u;
   }
   return NULL;
}

void
session_destroy(skt_t *lst, session_unit_t *u) {
   if (lst && u) {
      skt_remove(lst, u->sid);
   }
   if ( u ) {
      memset(u, 0, sizeof(*u));
      free(u);
   }
}
