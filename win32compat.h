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
#include <winsock2.h>
#include <errno.h>
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

#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN 1

struct rusage
{
	/* This data type stores various resource usage statistics. It has the 
	   following members, and possibly others: */

	struct timeval ru_utime;	/* Time spent executing user instructions. */
	struct timeval ru_stime;	/*Time spent in operating system code on behalf of processes. */
	long int ru_maxrss;			/* The maximum resident set size used, in kilobytes. That is, the maximum number of kilobytes of physical memory that processes used simultaneously. */
	long int ru_ixrss;			/* An integral value expressed in kilobytes times ticks of execution, which indicates the amount of memory used by text that was shared with other processes. */
	long int ru_idrss;			/* An integral value expressed the same way, which is the amount of unshared memory used for data. */
	long int ru_isrss;			/* An integral value expressed the same way, which is the amount of unshared memory used for stack space. */
	long int ru_minflt;			/* The number of page faults which were serviced without requiring any I/O. */
	long int ru_majflt;			/* The number of page faults which were serviced by doing I/O. */
	long int ru_nswap;			/* The number of times processes was swapped entirely out of main memory. */
	long int ru_inblock;		/* The number of times the file system had to read from the disk on behalf of processes. */
	long int ru_oublock;		/* The number of times the file system had to write to the disk on behalf of processes. */
	long int ru_msgsnd;			/* Number of IPC messages sent. */
	long int ru_msgrcv;			/* Number of IPC messages received. */
	long int ru_nsignals;		/* Number of signals received. */
	long int ru_nvcsw;			/* The number of times processes voluntarily invoked a context switch (usually to wait for some service). */
	long int ru_nivcsw;			/* The number of times an involuntary context switch took place (because a time slice expired, or another process of higher priority was scheduled).  */
};

/* Prototype stuff ********************************************************** */
extern int gettimeofday(struct timeval *time_Info, struct timezone *timezone_Info);
extern clock_t times(struct tms *__buffer);
extern int getrusage(int processes, struct rusage *rusage);

#endif