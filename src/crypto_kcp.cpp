/* 
 * Copyright (c) 2016 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include "plat_net.h"
#include "session_proto.h"
#include <iostream>

using namespace std;

/* fromo cloudwu's https://github.com/cloudwu/mptun/blob/master/mptun.c */

#define DEF_BUFF_SIZE MNET_BUF_SIZE

#ifndef DEF_TIME_DIFF
#define DEF_TIME_DIFF 1800
#endif

struct rc4_sbox {
   int i;
   int j;
   uint8_t sbox[256];
};

static void
rc4_init(struct rc4_sbox *rs, uint64_t seed) {
   rs->i=0;
   rs->j=0;
   int i;
   uint8_t k[8];
   for (i=0;i<8;i++) {
      k[i] = seed & 0xff;
      seed >>= 8;
   }
   for (i=0;i<256;i++) {
      rs->sbox[i] = (uint8_t)((i + k[i%8]) & 0xff);
   }
}

static void
rc4_encode(struct rc4_sbox *rs, const uint8_t *src, uint8_t *des, size_t sz) {
   size_t i;
   for (i=0;i<sz;i++) {
      rs->i = (rs->i + 1) % 256;
      rs->j = (rs->j + rs->sbox[rs->i]) % 256;
      uint8_t si = rs->sbox[rs->i];
      uint8_t sj = rs->sbox[rs->j];
      rs->sbox[rs->i] = sj;
      rs->sbox[rs->j] = si;
      uint8_t d = src[i] ^ rs->sbox[(si+sj) % 256];
      des[i] = d;
   }
}

uint64_t
rc4_hash_key(const char * str, int sz) {
   uint32_t djb_hash = 5381L;
   uint32_t js_hash = 1315423911L;

   int i;
   for (i=0;i<sz;i++) {
      uint8_t c = (uint8_t)str[i];
      djb_hash += (djb_hash << 5) + c;
      js_hash ^= ((js_hash << 5) + c + (js_hash >> 2));
   }

   return (uint64_t) djb_hash << 32 | js_hash;
}

// leftrotate function definition
#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

static uint64_t
hmac(uint64_t x, uint64_t y) {
// Constants are the integer part of the sines of integers (in radians) * 2^32.
   static const uint32_t k[64] = {
      0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee ,
      0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501 ,
      0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be ,
      0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821 ,
      0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa ,
      0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8 ,
      0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed ,
      0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a ,
      0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c ,
      0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70 ,
      0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05 ,
      0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665 ,
      0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039 ,
      0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1 ,
      0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1 ,
      0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
 
// r specifies the per-round shift amounts
   static const uint32_t r[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                                4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
 
   uint32_t w[16];
   uint32_t a, b, c, d, f, g, temp;
   int i;
 
   a = 0x67452301u;
   b = 0xefcdab89u;
   c = 0x98badcfeu;
   d = 0x10325476u;

   for (i=0;i<16;i+=4) {
      w[i] = (uint32_t)(x << 32);
      w[i+1] = (uint32_t)x;
      w[i+2] = (uint32_t)(y << 32);
      w[i+3] = (uint32_t)y;
   }

   for(i = 0; i<64; i++) {
      if (i < 16) {
         f = (b & c) | ((~b) & d);
         g = i;
      } else if (i < 32) {
         f = (d & b) | ((~d) & c);
         g = (5*i + 1) % 16;
      } else if (i < 48) {
         f = b ^ c ^ d;
         g = (3*i + 5) % 16; 
      } else {
         f = c ^ (b | (~d));
         g = (7*i) % 16;
      }

      temp = d;
      d = c;
      c = b;
      b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]);
      a = temp;

   }

   return (uint64_t)(a^b) << 32 | (c^d);
}

int
rc4_encrypt(const char *in, int sz, char *out, uint64_t key, time_t ti) {
   uint64_t h = rc4_hash_key(in, sz);
   uint32_t tmp;
   struct rc4_sbox rs;
   if (sz > DEF_BUFF_SIZE - 8) {
      cerr << "insufficient buffer size " << sz << endl;
      return -1;
   }
   key = hmac(key, ti);
   rc4_init(&rs, key);
   key ^= h;
   tmp = htonl(ti);
   memcpy(out, &tmp, 4);
   tmp = htonl((uint32_t)key ^ (uint32_t)(key >> 32));
   memcpy(out+4, &tmp, 4);
   rc4_encode(&rs, (const uint8_t *)in, (uint8_t *)out+8, sz);

   return sz + 8;
}

int
rc4_decrypt(const char *in, int sz, char *out, uint64_t key, time_t ti) {
   uint32_t pt, check;
   uint64_t h;
   struct rc4_sbox rs;
   sz -= 8;
   if (sz < 0) {
      cerr << "insufficient buffer size" << sz << endl;
      return -1;
   }

   memcpy(&pt, in, 4);
   memcpy(&check, in+4, 4);
   pt = ntohl(pt);
   check = ntohl(check);
   if (abs((int)(pt - ti)) > DEF_TIME_DIFF) {
      cerr << "over time " << DEF_TIME_DIFF << " seconds" << endl;
      return -1;
   }
   key = hmac(key, pt);
   rc4_init(&rs, key);

   rc4_encode(&rs, (const uint8_t *)in+8, (uint8_t *)out, sz);
   h = rc4_hash_key(out, sz);
   key ^= h;

   if (check != ((uint32_t)key ^ (uint32_t)(key >> 32))) {
      cerr << "key invalid" << endl;
      return -1;
   }
   return sz;
}
