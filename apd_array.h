/* 
   +----------------------------------------------------------------------+
   | APD Profiler & Debugger                                              |
   +----------------------------------------------------------------------+
   |  Copyright (c) 2003 The PHP Group                                    |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Sterling Hughes <sterling@php.net>                          |
   +----------------------------------------------------------------------+
*/

#ifndef APD_ARRAY_H
#define APD_ARRAY_H

typedef struct {
	void **blob;
	long size;
	long alloced;
	long blocksize;
	long largest;
} apd_array_t;

typedef void (*apd_array_dtor_t)(void *);

void apd_array_init(apd_array_t *, long, double);
int apd_array_set(apd_array_t *, long, void *);
void *apd_array_get(apd_array_t *, long);
void apd_array_clean(apd_array_t *, apd_array_dtor_t);
void apd_array_destroy(apd_array_t *, apd_array_dtor_t);

#endif

/**
 * Local Variables:
 * indent-tabs-mode: t
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 * vim600:fdm=marker
 * vim:noet:sw=4:ts=4
 */
