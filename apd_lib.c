/* 
   +----------------------------------------------------------------------+
   | APD Profiler & Debugger
   +----------------------------------------------------------------------+
   | Copyright (c) 2001-2003 Community Connect Inc.
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   |          George Schlossnagle <george@lethargy.org>                   |
   +----------------------------------------------------------------------+
*/

#include "apd_lib.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "zend.h"
#include "php.h"
#include "zend.h"

#ifndef PHP_WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

#undef DEBUG

/* apd_sprintf: safe, automatic sprintf */
char* apd_sprintf(const char* fmt, ...)
{
	char* newStr;
	va_list args;

	va_start(args, fmt);
	newStr = apd_sprintf_real(fmt, args);
	va_end(args);
	return newStr;
}

/* apd_sprintf_real: the meat of the safe, automatic sprintf */
char* apd_sprintf_real(const char* fmt, va_list args)
{
	char* newStr;
	int size = 1;
#ifdef va_copy
	va_list copy;
#endif
	newStr = (char*) emalloc(size);
	for (;;) {
		int n;
#ifdef va_copy
		va_copy(copy, args);
		n = vsnprintf(newStr, size, fmt, copy);
		va_end(copy);
#else
		n = vsnprintf(newStr, size, fmt, args);
#endif
		if (n > -1 && n < size) {
			break;
		}
		if(n < 0 ) {
			size *= 2;
		}
		else {
			size = n+1;
		}
		newStr = (char*) erealloc(newStr, size);
	}
	va_end(args);

	return newStr;
}

void timevaldiff(struct timeval *a, struct timeval *b, struct timeval *result)
{
	result->tv_sec = a->tv_sec - b->tv_sec;          
	result->tv_usec = a->tv_usec - b->tv_usec; 
	if (result->tv_usec < 0) {
		--(result->tv_sec); 
		result->tv_usec += 1000000;
	}
}

/**
 * Local Variables:
 * indent-tabs-mode: t
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 * vim600:fdm=marker
 * vim:noet:sw=4:ts=4
 */
