/*****************************************************************************
 *                                                                           *
 * DH_TIME.C                                                                 *
 *                                                                           *
 * Freely redistributable and modifiable.  Use at your own risk.             *
 *                                                                           *
 * Copyright 1994 The Downhill Project                                       *
 *                                                                           *
 * Modified by Shane Caraveo for use with PHP                                *
 *                                                                           *
 * Modified by Markus Fischer for the APD project                            *
 *                                                                           *
 *****************************************************************************/

/* $Id$ */

#include "win32compat.h"

int FiletimeToTimeval(FILETIME ft, struct timeval *time_Info) 
{
	__int64 ff;

    ff = *(__int64*)(&ft);
    time_Info->tv_sec = (int)(ff/(__int64)10000000-(__int64)11644473600);
    time_Info->tv_usec = (int)(ff % 10000000)/10;
    return 0;
}

clock_t times (struct tms *__buffer)
{
	HANDLE cp;
    FILETIME CreationTime, ExitTime, KernelTime, UserTime;
	struct timeval time_Info;
	/* If times fails, a -1 is returned and errno is	set to indicate	the error. */
	if (!__buffer) return -1;
    cp = GetCurrentProcess();

    GetProcessTimes(cp, &CreationTime, &ExitTime, &KernelTime, &UserTime);
	/*
     tms_utime is the CPU time used while executing instructions in the	user
     space of the calling process.
	*/
	FiletimeToTimeval(UserTime,&time_Info);
    __buffer->tms_utime = (time_Info.tv_sec + (time_Info.tv_usec / 10000000)) * CLOCKS_PER_SEC;
	/*
     tms_stime is the CPU time used by the system on behalf of the calling
     process.
	*/
	FiletimeToTimeval(KernelTime,&time_Info);
    __buffer->tms_stime = (time_Info.tv_sec + (time_Info.tv_usec / 10000000)) * CLOCKS_PER_SEC;

	/* Note: Windows has such a thing as a process tree, which could 
	   possibly be used to derive these values, but since we dont use them,
	   no need to bother now. */
	/*
     tms_cutime	is the sum of the tms_utime and	the tms_cutime of the child
     processes.
	*/
	__buffer->tms_cutime = 0;
	/*
     tms_cstime	is the sum of the tms_stime and	the tms_cstime of the child
     processes.
	*/
	__buffer->tms_cstime = 0;
    /*
	Upon successful completion, times returns the elapsed real	time, in clock
    ticks, from an arbitrary point in the past	(e.g., system start-up time).
	*/
	return GetTickCount();
}

int getrusage(int processes, struct rusage *rusage)
{
	HANDLE cp;
    FILETIME CreationTime, ExitTime, KernelTime, UserTime;
	/* If times fails, a -1 is returned and errno is	set to indicate	the error. */

    if (rusage == (struct rusage *) NULL) {
        errno = EFAULT;
        return -1;
    }

	memset(rusage, 0, sizeof(rusage));
    cp = GetCurrentProcess();
	GetProcessTimes(cp, &CreationTime, &ExitTime, &KernelTime, &UserTime);

    switch (processes) {
    case RUSAGE_SELF:
		FiletimeToTimeval(UserTime,&rusage->ru_utime);
		FiletimeToTimeval(KernelTime,&rusage->ru_stime);
        break;
	/*
		XXX no support for children times yet
    case RUSAGE_CHILDREN:
        u = tms.tms_cutime;
        s = tms.tms_cstime;
        break;
	*/
    default:
        errno = EINVAL;
        return -1;
    }
    return 0;
}

