/* 
 * Copyright (c) 2015 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef M_LIST_H
#define M_LIST_H

typedef enum {
   LST_FIRST,
   LST_LAST,
} lst_direction_t;

typedef struct s_lst lst_t;
typedef struct s_lst_node lst_node_t;

typedef struct s_lst_iter {
   lst_t *lst;
   lst_node_t *node;
   lst_direction_t from;
} lst_iter_t;

/* list: (first, ..., last), (prev, next) */

lst_t* lst_create();

void lst_destroy(lst_t*);

int lst_fnode_count(lst_t*);
void lst_fnode_keep(lst_t*, int count);

int lst_count(lst_t*);

void* lst_first(lst_t*);
void* lst_last(lst_t*);

lst_node_t* lst_pushf(lst_t*, void*);
lst_node_t* lst_pushl(lst_t*, void*);

void* lst_popf(lst_t*);
void* lst_popl(lst_t*);

void* lst_remove(lst_t*, lst_node_t*);

/* iterater operation */

lst_iter_t* lst_iter_init(lst_t*, lst_iter_t*, lst_direction_t);
lst_iter_t* lst_iter_next(lst_iter_t*);

lst_node_t* lst_iter_insert_prev(lst_t*, lst_iter_t*, void*);
lst_node_t* lst_iter_insert_next(lst_t*, lst_iter_t*, void*);

void* lst_iter_remove(lst_iter_t*);
void* lst_iter_data(lst_iter_t*);

#define lst_foreach(it, lst)                                            \
   for (lst_iter_t _##it, *it=lst_iter_init(lst, &_##it, LST_FIRST); (it=lst_iter_next(it));)

#define lst_foreach_r(it, lst)                                          \
   for (lst_iter_t _##it, *it=lst_iter_init(lst, &_##it, LST_LAST); (it=lst_iter_next(it));)

#endif
