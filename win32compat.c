#include "win32compat.h"

int gettimeofday(struct timeval *tv, void *dummy) {
	SYSTEMTIME s;

	GetSystemTime(&s);
	tv->tv_sec = s.wSecond;
	tv->tv_usec = s.wMilliseconds * 1000;
	return 0;
}

