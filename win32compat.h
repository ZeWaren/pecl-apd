#ifndef __WIN32COMPAT_H_
#define __WIN32COMPAT_H_

/*****************************************************************************
 *                                                                           *
 * win32compat                                                               *
 *                                                                           *
 * Freely redistributable and modifiable.  Use at your own risk.             *
 *                                                                           *
 * Copyright 1994 The Downhill Project                                       *
 *                                                                           *
 * Modified by Shane Caraveo for PHP                                         *
 *                                                                           *
 * Modified by Markus Fischer for the APD project                            *
 *                                                                           *
 *****************************************************************************/

/* $Id$ */


/* Include stuff ************************************************************ */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>
#include <time.h>

/* Struct stuff ************************************************************* */
typedef long clock_t;
#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 1000
#endif

struct tms
{
    clock_t tms_utime;          /* User CPU time.  */
    clock_t tms_stime;          /* System CPU time.  */

    clock_t tms_cutime;         /* User CPU time of dead children.  */
    clock_t tms_cstime;         /* System CPU time of dead children.  */
};


/* Prototype stuff ********************************************************** */
extern int gettimeofday(struct timeval *time_Info, struct timezone *timezone_Info);
extern clock_t times (struct tms *__buffer);

#endif