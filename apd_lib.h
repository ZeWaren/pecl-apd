/* 
   +----------------------------------------------------------------------+
   | APD Profiler & Debugger
   +----------------------------------------------------------------------+
   | Copyright (c) 2001-2002 Community Connect Inc.
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

#ifndef INCLUDED_APD_LIB
#define INCLUDED_APD_LIB

#include <stdlib.h>
#ifndef PHP_WIN32
#include <sys/time.h>
#endif
#include "zend.h"

/* wrappers for memory allocation routines */

extern char* apd_sprintf(const char* fmt, ...);
extern char* apd_sprintf_real(const char* fmt, va_list args);

/* timeval functions */
extern void timevaldiff(struct timeval* a, struct timeval* b, struct timeval* result);

#endif
