// 
// 
// Copyright (c) 2018 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rs_kcp.h"
#include "m_rs.h"
#include "m_mem.h"

#define RS_PARAM_COUNT 1

struct s_rs_param {
   int k, t2;
};

static struct s_rs_param
g_rs_params[RS_PARAM_COUNT] = {
   // { 223, 32 },
   // { 112, 16 },
   // {  56, 8 },
   {  11, 2 },
};

struct s_rskcp {
  rs_t *rs_ins[RS_PARAM_COUNT];
   s_rs_param *params;
};

rskcp_t*
rskcp_create(void) {
   rskcp_t *rt = (rskcp_t*)mm_malloc(sizeof(rskcp_t));
   if (rt) {
      rt->params = g_rs_params;
      for (int i=0; i<RS_PARAM_COUNT; i++) {
         struct s_rs_param *p = &rt->params[i];
         rt->rs_ins[i] = rs_init(p->k, p->t2);
      }
   }
   return rt;
}

void
rskcp_release(rskcp_t *rt) {
   if (rt) {
      for (int i=0; i<RS_PARAM_COUNT; i++) {
         rs_fini(rt->rs_ins[i]);
      }      
      mm_free(rt);
   }
}

static int
_rs_param_enc(rskcp_t *rt, int dlen) {
   for (int i=0; i<RS_PARAM_COUNT; i++) {
      struct s_rs_param *p = &rt->params[i];
      if (dlen >= p->k) {
         return i;
      }
   }
   return RS_PARAM_COUNT - 1;
}

static int
_rs_param_dec(rskcp_t *rt, int dlen) {
   for (int i=0; i<RS_PARAM_COUNT; i++) {
      struct s_rs_param *p = &rt->params[i];
      if (dlen >= (p->k + p->t2)) {
         return i;
      }
   }
   return RS_PARAM_COUNT -1;
}


// return aligned data_len 
int
rskcp_enc_info(rskcp_t *rt, int raw_len, int *parity_len) {
   if (!rt || raw_len<=0 || !parity_len) {
      return 0;
   }

   int di=0, pi=0;
   do {
      int idx = _rs_param_enc(rt, raw_len);
      struct s_rs_param *p = &rt->params[idx];
      di += p->k;
      pi += p->t2;
      raw_len -= p->k;
   } while (raw_len > 0);

   *parity_len = pi;
   return di;
}

// return aligned raw_len
int
rskcp_dec_info(rskcp_t *rt, int data_len) {
   if (!rt || data_len<=0) {
      return 0;
   }

   int di=0, pi=0;
   do {
      int idx = _rs_param_dec(rt, data_len);
      struct s_rs_param *p = &rt->params[idx];
      di += p->k;
      pi += p->t2;
      data_len -= p->k + p->t2;
   } while (data_len > 0);

   return di;
}

int
rskcp_encode(rskcp_t *rt, unsigned char *raw, int raw_len, unsigned char *parity) {
   if (!rt || !raw_len || raw_len<=0 || !parity) {
      return 0;
   }

   int ri=0, pi=0, ret=0;
   do {
      int idx = _rs_param_enc(rt, raw_len);
      ret = rs_encode(rt->rs_ins[idx], &raw[ri], &parity[pi]);
      if ( ret ) {
         struct s_rs_param *p = &rt->params[idx];
         ri += p->k;
         pi += p->t2;
         raw_len -= p->k;
      }
   } while (ret && raw_len>0);

   return ret;
}

int
rskcp_decode(rskcp_t *rt, unsigned char *data, int data_len, unsigned char *parity) {
   if (!rt || !data || data_len<=0 || !parity) {
      return 0;
   }

   int di=0, pi=0, ret=0, idx=0;
   do {
      idx = _rs_param_dec(rt, data_len);
      ret = rs_decode(rt->rs_ins[idx], &data[di], &parity[pi]);
      if ( ret ) {
         struct s_rs_param *p = &rt->params[idx];         
         di += p->k;
         pi += p->t2;
         data_len -= p->k + p->t2;
      }
   } while (ret && data_len>0);

   return ret;
}

#undef RS_PARAM_COUNT
