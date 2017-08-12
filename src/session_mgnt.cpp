/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "session_mgnt.h"
#include <stdlib.h>

session_unit_t*
session_create(lst_t *lst, unsigned sid, chann_t *tcp, void *opaque) {
   if (lst && tcp) {
      session_unit_t *u = (session_unit_t*)malloc(sizeof(*u));
      if (u) {
         u->sid = sid;
         u->tcp = tcp;
         u->opaque = opaque;
         u->node = lst_pushl(lst, u);
         return u;
      }
   }
   return NULL;
}

session_unit_t*
session_find_sid(lst_t *lst, unsigned sid) {
   if (lst) {
      lst_foreach(it, lst) {
         session_unit_t *u = (session_unit_t*)lst_iter_data(it);
         if (u->sid == sid) {
            return u;
         }
      }
   }
   return NULL;
}

void session_destroy(lst_t *lst, session_unit_t *u) {
   if (lst && u) {
      lst_remove(lst, (lst_node_t*)u->node);
      free(u);
   }
}
