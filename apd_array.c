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

#include <stdlib.h>
#include "apd_array.h"
#include "php.h"

/**
 * Its not a queue, its not a stack, its a sequential a cheap array 
 * designed so that one uses sequential indexes for writes and random 
 * access on gets.
 * 
 * It handles reallocation and bounds checking, but that's about it.
 *
 * Its well suited for apd's needs, but use this code at your own peril, 
 * as general usage will certainly come back and bite you in the butt.
 *
 * -Sterling, 06/2003
 */

void 
apd_array_init(apd_array_t *a, long alloc, double blocksize)
{
	a->alloced = alloc;
	a->size = 0;
	a->blocksize = blocksize;
	/**
	 * largest is used by apd_array_append()
	 * initial element is -1, so if its used in an empty array
	 * 0 is the initial index
	 */
	a->largest = -1;
	
	a->blob = (void **) ecalloc(sizeof(void *), a->alloced);
}

inline void
_grow_array(apd_array_t *a)
{
	long oldalloc;
	long i;

	oldalloc = a->alloced;
	
	a->alloced = (long) (a->blocksize * (double) a->alloced);
	a->blob = (void **) erealloc(a->blob, a->alloced * sizeof(void *));
	
	for (i = oldalloc; i < a->alloced; i++) {
		a->blob[i] = NULL;
	}
}
	
int 
apd_array_append(apd_array_t *a, void *ptr)
{
	apd_array_set(a, a->largest+1, ptr);
}

int
apd_array_set(apd_array_t *a, long index, void *ptr)
{
	if (a == NULL || index < 0) {
		return 0;
	}

	if (a->alloced < index) {
		_grow_array(a);
	}

	a->blob[index] = ptr;
	if (ptr) {
		a->size++;
		if (a->largest < index) {
			a->largest = index;
		}
	} else {
		long i;
		
		a->size--;

		for (i = 0; i < a->alloced; i++) {
			if (a->blob[i] != NULL) {
				a->largest = i;
			}
		}
	}
}

void *
apd_array_get(apd_array_t *a, long index)
{
	if (a == NULL || index > a->alloced || index < 0) {
		return NULL;
	}

	return a->blob[index];
}

void
apd_array_clean(apd_array_t *a, apd_array_dtor_t dtor)
{
	long i;

	for (i = 0; i < a->alloced; i++) {
		if (a->blob[i] != NULL) {
			dtor(a->blob[i]);
			a->blob[i] = NULL;
		}
	}
}

void 
apd_array_destroy(apd_array_t *a, apd_array_dtor_t dtor)
{
	if (dtor) {
		apd_array_clean(a, dtor);
	}

	efree(a->blob);
	memset(a, 0, sizeof(apd_array_t));
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
