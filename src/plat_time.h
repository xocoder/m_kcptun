/* 
 * Copyright (c) 2015 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef PLAT_TIME_H
#define PLAT_TIME_H

#include <stdint.h>

#define MTIME_MICRO_PER_SEC 1000000
#define MTIME_MILLI_PER_SEC 1000

int64_t mtime_current(void);    /* in micro sec */
void mtime_sleep(int millisecond);

#endif
