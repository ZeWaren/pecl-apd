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
#include <sys/time.h>
#include "zend.h"

/* wrappers for memory allocation routines */

extern void* apd_emalloc (size_t n);
extern void* apd_erealloc(void* p, size_t n);
extern void  apd_efree   (void* p);
extern char* apd_estrdup (const char* s);
extern char* apd_copystr (const char* s, int len);

/* simple display facility */

extern void apd_eprint(char *fmt, ...);
extern void apd_dprint(char *fmt, ...);

/* simple timer facility */

extern void   apd_timerstart (void);
extern void   apd_timerstop  (void);
extern double apd_timerreport(void);

/* string routines */

extern void  apd_strcat (char** dst, int* curSize, const char* src);
extern void apd_strncat(char** dst, int* curSize, const char* src, int srcLen);
extern char* apd_sprintf(const char* fmt, ...);
extern char* apd_sprintf_real(const char* fmt, va_list args);
/* sprintf's to the end of a string.  reallocates automatically */
extern char* apd_sprintcatf(char** dst, const char* fmt, ...);
/* indents a string by the specified amount */
extern char* apd_indent(char **dst, int spaces);
extern char* apd_strtac(char **dst, char *src);


/* resource access routines */

extern int __apd_dump_regular_resources(zval *array TSRMLS_DC);
extern int __apd_dump_persistent_resources(zval *array TSRMLS_DC);

/* timeval functions */
extern void timevaldiff(struct timeval* a, struct timeval* b, struct timeval* result);

#endif
