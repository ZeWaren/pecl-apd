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
#include <winsock.h>
#include <time.h>

/* Struct stuff ************************************************************* */
struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};


/* Prototype stuff ********************************************************** */
extern int gettimeofday(struct timeval *time_Info, struct timezone *timezone_Info);

#endif