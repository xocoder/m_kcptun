/* 
 * Copyright (c) 2015 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef __APPLE__
#import "TargetConditionals.h"

#if TARGET_OS_IPHONE
#define PLAT_OS_IOS

#elif TARGET_OS_MAC
#define PLAT_OS_MAC

#endif  /* __APPLE__ */

#elif defined(_WIN32) || defined(_WIN64)
#define PLAT_OS_WIN

#else
#define PLAT_OS_LINUX
#endif

