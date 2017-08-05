/* 
 * Copyright (c) 2015 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "plat_os.h"

#ifdef PLAT_OS_WIN
#include <windows.h>
#else

#ifdef PLAT_OS_LINUX
#define _BSD_SOURCE
#endif

#include <sys/time.h>
#include <unistd.h>

#endif
#include "plat_time.h"

/* micro second */
int64_t mtime_current(void) {
#ifdef PLAT_OS_WIN
   FILETIME ft;
   int64_t t;
   GetSystemTimeAsFileTime(&ft);
   t = (int64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
   return t / 10 - 11644473600000000; /* Jan 1, 1601 */
#else
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

void mtime_sleep(int millisecond) {
#ifdef PLAT_OS_WIN
   Sleep(millisecond);
#else
   usleep(millisecond * 1000);
#endif
}
