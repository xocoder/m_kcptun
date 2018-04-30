/* 
 * Copyright (c) 2018 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

/* m_foundation import example
 */

/* for all */
#define M_FOUNDATION_IMPORT_CRYPTO 1
#define M_FOUNDATION_IMPORT_MODEL  1
#define M_FOUNDATION_IMPORT_PLAT   1

/* crypto module */
#ifdef M_FOUNDATION_IMPORT_CRYPTO
#define M_FOUNDATION_IMPORT_CRYPTO_PRNG    1
#define M_FOUNDATION_IMPORT_CRYPTO_RC4     1
#define M_FOUNDATION_IMPORT_CRYPTO_XOR64   1
#endif  /* M_FOUNDATION_IMPORT_CRYPTO */


/* model module */
#ifdef M_FOUNDATION_IMPORT_MODEL

#define M_FOUNDATION_IMPORT_MODEL_MEM 1

#define M_FOUNDATION_IMPORT_MODEL_LIST (M_FOUNDATION_IMPORT_MODEL_MEM)

#define M_FOUNDATION_IMPORT_MODEL_SKIPLIST (M_FOUNDATION_IMPORT_MODEL_MEM && \
                                            M_FOUNDATION_IMPORT_CRYPTO_PRNG)

#define M_FOUNDATION_IMPORT_MODEL_TIMER (M_FOUNDATION_IMPORT_MODEL_LIST && \
                                         M_FOUNDATION_IMPORT_MODEL_SKIPLIST)

#endif  /* M_FOUNDATION_IMPORT_MODEL */



/* plat module */
#ifdef M_FOUNDATION_IMPORT_PLAT

/* time for win/nix */
#define M_FOUNDATION_IMPORT_PLAT_TIME    1

#endif  /* M_FOUNDATION_IMPORT_PLAT */
