#include "win32compat.h"


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

 /**
  *
  * 04-Feb-2001
  *   - Added patch by "Vanhanen, Reijo" <Reijo.Vanhanen@helsoft.fi>
  *     Improves accuracy of msec
  */

int getfilesystemtime(struct timeval *time_Info) 
{
FILETIME ft;
__int64 ff;

    GetSystemTimeAsFileTime(&ft);   /* 100 ns blocks since 01-Jan-1641 */
                                    /* resolution seems to be 0.01 sec */ 
    ff = *(__int64*)(&ft);
    time_Info->tv_sec = (int)(ff/(__int64)10000000-(__int64)11644473600);
    time_Info->tv_usec = (int)(ff % 10000000)/10;
    return 0;
}

int gettimeofday(struct timeval *time_Info, struct timezone *timezone_Info)
{

	static struct timeval starttime = {0, 0};
	static __int64 lasttime = 0;
	static __int64 freq = 0;
	__int64 timer;
	LARGE_INTEGER li;
	BOOL b;
	double dt;

	/* Get the time, if they want it */
	if (time_Info != NULL) {
		if (starttime.tv_sec == 0) {
            b = QueryPerformanceFrequency(&li);
            if (!b) {
                starttime.tv_sec = -1;
            }
            else {
                freq = li.QuadPart;
                b = QueryPerformanceCounter(&li);
                if (!b) {
                    starttime.tv_sec = -1;
                }
                else {
                    getfilesystemtime(&starttime);
                    timer = li.QuadPart;
                    dt = (double)timer/freq;
                    starttime.tv_usec -= (int)((dt-(int)dt)*1000000);
                    if (starttime.tv_usec < 0) {
                        starttime.tv_usec += 1000000;
                        --starttime.tv_sec;
                    }
                    starttime.tv_sec -= (int)dt;
                }
            }
        }
        if (starttime.tv_sec > 0) {
            b = QueryPerformanceCounter(&li);
            if (!b) {
                starttime.tv_sec = -1;
            }
            else {
                timer = li.QuadPart;
                if (timer < lasttime) {
                    getfilesystemtime(time_Info);
                    dt = (double)timer/freq;
                    starttime = *time_Info;
                    starttime.tv_usec -= (int)((dt-(int)dt)*1000000);
                    if (starttime.tv_usec < 0) {
                        starttime.tv_usec += 1000000;
                        --starttime.tv_sec;
                    }
                    starttime.tv_sec -= (int)dt;
                }
                else {
                    lasttime = timer;
                    dt = (double)timer/freq;
                    time_Info->tv_sec = starttime.tv_sec + (int)dt;
                    time_Info->tv_usec = starttime.tv_usec + (int)((dt-(int)dt)*1000000);
                    if (time_Info->tv_usec > 1000000) {
                        time_Info->tv_usec -= 1000000;
                        ++time_Info->tv_sec;
                    }
                }
            }
        }
        if (starttime.tv_sec < 0) {
            getfilesystemtime(time_Info);
        }

	}
	/* Get the timezone, if they want it 
	if (timezone_Info != NULL) {
		_tzset();
		timezone_Info->tz_minuteswest = _timezone;
		timezone_Info->tz_dsttime = _daylight;
	}
	 And return */
	return 0;
}

