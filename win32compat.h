#ifndef __WIN32COMPAT_H_
#define __WIN32COMPAT_H_

#include <windows.h>

int gettimeofday(struct timeval *tv, void *dummy);

#endif
