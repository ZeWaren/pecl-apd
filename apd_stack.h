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

#ifndef INCLUDED_APD_STACK
#define INCLUDED_APD_STACK

#define T apd_stack_t*
typedef struct apd_stack_t apd_stack_t;

extern T		apd_stack_create		();
extern void		apd_stack_destroy		(T stack);
extern void		apd_stack_push			(T stack, void* ptr);
extern void*	apd_stack_pop			(T stack);
extern int		apd_stack_isempty		(T stack);
extern void**	apd_stack_toarray		(T stack);
extern int		apd_stack_getsize		(T stack);
extern void		apd_stack_apply			(T stack, void (*funcPtr)(void*));

#undef T
#endif
