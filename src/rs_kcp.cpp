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
#include "cm256.h"
#include "m_mem.h"

struct s_rskcp {
   cm256_encoder_params param;
   cm256_block blocks[256];
};

rskcp_t*
rskcp_create(int data_bytes, int parity_bytes) {
   if ( cm256_init() ) {
      return (rskcp_t*)0;
   }

   rskcp_t *rt = (rskcp_t*)mm_malloc(sizeof(rskcp_t));
   if (rt) {
      cm256_encoder_params p = { data_bytes, parity_bytes, 1 };
      rt->param = p;
      for (int i=0; i<(data_bytes + parity_bytes); i++) {
         rt->blocks[i].Index = i;
      }
   }
   return rt;
}

void
rskcp_release(rskcp_t *rt) {
   if (rt) {
      mm_free(rt);
   }
}

// return aligned data_len 
int
rskcp_enc_info(rskcp_t *rt, int raw_len, int *parity_len) {
   if (!rt || raw_len<=0 || !parity_len) {
      return 0;
   }

   cm256_encoder_params *p = &rt->param;
   int di=0, pi=0;
   do {
      di += p->OriginalCount;
      pi += p->RecoveryCount;
   } while (di < raw_len);

   *parity_len = pi;
   return di;
}

// return aligned raw_len
int
rskcp_dec_info(rskcp_t *rt, int data_len) {
   if (!rt || data_len<=0) {
      return 0;
   }

   cm256_encoder_params *p = &rt->param;
   int di=0, pi=0;
   do {
      di += p->OriginalCount;
      pi += p->RecoveryCount;
   } while (data_len < di + pi);

   return di;
}

static inline void
_rskcp_fill_blocks(cm256_block *blocks, unsigned char *data, int count) {
   for (int i=0; i<count; i++) {
      blocks[i].Block = data + i;
   }
}

int
rskcp_encode(rskcp_t *rt, unsigned char *raw, int raw_len, unsigned char *parity) {
   if (!rt || !raw_len || raw_len<=0 || !parity) {
      return 0;
   }

   cm256_encoder_params *p = &rt->param;
   cm256_block *blocks = rt->blocks;
   int ri=0, pi=0, ret=0;
   
   do {
      _rskcp_fill_blocks(blocks, &raw[ri], p->OriginalCount);

      ret = cm256_encode(*p, blocks, &parity[pi]);
      if ( !ret ) {
         ri += p->OriginalCount;
         pi += p->RecoveryCount;
      }
   } while (!ret && raw_len<ri);

   return !ret;
}

int
rskcp_decode(rskcp_t *rt, unsigned char *data, int data_len, unsigned char *parity) {
   if (!rt || !data || data_len<=0 || !parity) {
      return 0;
   }

   cm256_encoder_params *p = &rt->param;
   cm256_block *blocks = rt->blocks;
   int di=0, pi=0, ret=0;
   
   do {
      _rskcp_fill_blocks(blocks, &data[di], p->OriginalCount);
      _rskcp_fill_blocks(blocks + p->OriginalCount, &parity[pi], p->RecoveryCount);
      
      ret = cm256_decode(*p, blocks);
      if ( !ret ) {
         di += p->OriginalCount;
         pi += p->RecoveryCount;
      }
   } while (!ret && data_len<di+pi);
   
   return !ret;
}
