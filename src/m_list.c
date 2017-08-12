/* 
 * Copyright (c) 2015 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include <assert.h>

#include "m_list.h"

#define M_FREE_NODE_RESERVE 1  /* 0 for disable free node list */

struct s_lst_node {
   void *data;
   struct s_lst_node *prev;
   struct s_lst_node *next;
};

struct s_lst {
   int count;                   /* valid data node */
   int free_count;              /* free node */
   lst_node_t *first;           /* first in list */
   lst_node_t *last;            /* last in list */
   lst_node_t *free;            /* free node list */
};

static inline void
_lst_node_add_free(lst_t *lst, lst_node_t *n) {
   n->next = lst->free;
   lst->free = n;
   lst->free_count++;
}

static inline lst_node_t*
_lst_node_remove_free(lst_t *lst) {
   lst_node_t *n = lst->free;
   lst->free = n->next;
   lst->free_count--;
   return n;
}

static inline lst_node_t*
_lst_node_new(lst_t *lst, void *data) {
   lst_node_t *n = NULL;
   if (lst->free_count > 0) {
      n = _lst_node_remove_free(lst);
      memset(n, 0, sizeof(*n));
   }
   else {
      n = (lst_node_t*)malloc(sizeof(*n));
      /* assert(n); */
   }
   n->data = data;
   return n;
}

static inline void*
_lst_node_delete(lst_t *lst, lst_node_t *n) {
   void *data = n->data;

   if (lst->first == n) lst->first = n->next;
   if (lst->last == n) lst->last = n->prev;

   if (n->prev) n->prev->next = n->next;
   if (n->next) n->next->prev = n->prev;

   if ( M_FREE_NODE_RESERVE ) {
      _lst_node_add_free(lst, n);
   }
   else {
      free(n);
   }
   lst->count--;
   return data;
}

lst_t* lst_create(void) {
   lst_t *lst = (lst_t*)malloc(sizeof(*lst));
   /* assert(lst); */
   /* memset(lst, 0, sizeof(*lst)); */
   return lst;
}

int lst_fnode_count(lst_t *lst) {
   return lst ? lst->free_count : -1;
}

void lst_fnode_keep(lst_t *lst, int count) {
   if (lst && count>=0) {
      while (lst->free_count > count) {
         lst_node_t *n = lst->free;
         lst->free = n->next;
         lst->free_count--;
         free(n);
      }
   }
}

void lst_destroy(lst_t *lst) {
   if (lst) {
      lst_node_t *f = lst->first;
      while ( f ) {
         lst_node_t *n = f->next;
         free(f);
         f = n;
      }
      lst_fnode_keep(lst, 0);
      free(lst);
   }
}

int lst_count(lst_t *lst) { return lst ? lst->count : -1; }
void* lst_data(lst_iter_t *it) { return (it && it->node) ? it->node->data : NULL; }

void* lst_first(lst_t *lst) { return (lst && lst->first) ? lst->first->data : NULL; }
void* lst_last(lst_t *lst) { return (lst && lst->last) ? lst->last->data : NULL; }

lst_node_t* lst_pushf(lst_t *lst, void *data) {
   if ( lst ) {
      lst_node_t *n = _lst_node_new(lst, data);
      if (lst->count == 0) {
         lst->first = lst->last = n;
      }
      else {
         lst->first->prev = n;
         n->next = lst->first;
         lst->first = n;
      }
      lst->count++;
      return n;
   }
   return NULL;
}

lst_node_t* lst_pushl(lst_t *lst, void *data) {
   if ( lst ) {
      lst_node_t *n = _lst_node_new(lst, data);
      if (lst->count == 0) {
         lst->first = lst->last = n;
      }
      else {
         lst->last->next = n;
         n->prev = lst->last;
         lst->last = n;
      }
      lst->count++;
      return n;
   }
   return NULL;
}

void* lst_popf(lst_t *lst) {
   if (lst && (lst->count>0)) {
      return _lst_node_delete(lst, lst->first);
   }
   return NULL;
}

void* lst_popl(lst_t *lst) {
   if (lst && (lst->count>0)) {
      return _lst_node_delete(lst, lst->last);
   }
   return NULL;
}

void* lst_remove(lst_t *lst, lst_node_t *n) {
   if (lst && n) {
      return _lst_node_delete(lst, n);
   }
   return NULL;
}

/* iterater */

lst_iter_t*
lst_iter_init(lst_t *lst, lst_iter_t *it, lst_direction_t from) {
   if ((lst->count>0) && it) {
      it->lst = lst;
      it->from = from;
      it->node = NULL;
      return it;
   }
   return NULL;
}

lst_iter_t* lst_iter_next(lst_iter_t *it) {
   if (it && it->lst) {
      if (it->node == NULL) {
         it->node = (it->from==LST_FIRST) ? it->lst->first : it->lst->last;
      }
      else {
         it->node = (it->from==LST_FIRST) ? it->node->next : it->node->prev;
      }
      if (it->node == NULL) {
         it->lst = NULL;
         return NULL;
      }
      return it;
   }
   return NULL;
}

void* lst_iter_remove(lst_iter_t *it) {
   if (it && it->lst && it->node) {
      lst_t *lst = it->lst;
      lst_node_t *n = it->node;
      if (lst_count(lst) <= 1) {
         it->lst = NULL;
         it->node = NULL;
      }
      else {
         if (it->from == LST_FIRST) it->node = it->node->prev;
         else it->node = it->node->next;
      }
      return _lst_node_delete(lst, n);
   }
   return NULL;
}

void* lst_iter_data(lst_iter_t *it) {
   return (it && it->node) ? it->node->data : NULL;
}

lst_node_t* lst_iter_insert_next(lst_t *lst, lst_iter_t *it, void *data) {
   if (lst && it && it->node) {
      lst_node_t *n = it->node;
      lst_node_t *m = _lst_node_new(lst, data);
      if (n->next) {
         n->next->prev = m;
         m->next = n->next;
      }
      else {
         lst->last = m;
      }
      n->next = m;
      m->prev = n;
      lst->count++;
      return m;
   }
   return NULL;
}

lst_node_t* lst_iter_insert_prev(lst_t *lst, lst_iter_t *it, void *data) {
   if (lst && it && it->node) {
      lst_node_t *n = it->node;
      lst_node_t *m = _lst_node_new(lst, data);
      if (n->prev) {
         m->prev = n->prev;
         n->prev->next = m;
      }
      else {
         lst->first = m;
      }
      n->prev = m;
      m->next = n;
      lst->count++;
      return m;
   }
   return NULL;
}

#ifdef LIST_TEST
#include <stdio.h>
int main(void) {
   int i=0, a[18] = {0};
   lst_t *lst = lst_create();
   for (i=0; i<18; i++) {
      a[i] = i;
      if (i < 15) {
         lst_pushf(lst, &a[i]);
      }
   }
   lst_popf(lst);
   lst_popl(lst);
   lst_foreach(e, lst) {
      int *c = (int*)lst_iter_data(e);
      if (*c == 0) {
         lst_iter_remove(e);
      }
      printf("a=%d\n", *c);
   }
   printf("-----\n");
   lst_foreach_r(it, lst) {
      int *c = (int*)lst_iter_data(it);
      printf("a=%d\n", *c);
   }
   printf("-----\n");
   lst_foreach_r(v, lst) {
      int *c = (int*)lst_iter_data(v);
      if (*c < 10)
         lst_iter_remove(v);
   }
   lst_foreach_r(f, lst) {
      int *c = (int*)lst_iter_data(f);
      printf("a=%d\n", *c);
   }
   printf("free_count=%d\n", lst->free_count);
   lst_destroy(lst);
   return 0;
}
#endif
