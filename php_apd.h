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

#ifndef PHP_APD_H
#define PHP_APD_H

#if PHP_WIN32
#include "config.w32.h"
#else
#include "php_config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "php_globals.h"

#include "zend.h"
#include "zend_API.h"
#include "zend_execute.h"
#include "zend_compile.h"
#include "zend_extensions.h"
 
#ifndef PHP_WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>
#else /* windows */
/* these are from ext/socket -- probably worth just copying the files into apd? */
# include <winsock.h>
# include "php_sockets.h"
# include "php_sockets_win.h"
# define IS_INVALID_SOCKET(a)  (a->bsd_socket == INVALID_SOCKET)
#endif

// ---------------------------------------------------------------------------
/* functions declared in php_apd.c */
void printUnsortedSummary(struct timeval TSRMLS_DC);
void apd_dump_fprintf(const char* fmt, ...);
void apd_interactive ();
// ---------------------------------------------------------------------------
// Required Declarations
// ---------------------------------------------------------------------------

/* Declarations of functions to be exported. */
PHP_FUNCTION(apd_callstack);
PHP_FUNCTION(apd_cluck);
PHP_FUNCTION(apd_croak);
PHP_FUNCTION(apd_dump_regular_resources);
PHP_FUNCTION(apd_dump_persistent_resources);
PHP_FUNCTION(override_function);
PHP_FUNCTION(rename_function);
PHP_FUNCTION(dump_function_table);
PHP_FUNCTION(apd_set_session_trace);
PHP_FUNCTION(apd_set_pprof_trace);
PHP_FUNCTION(apd_set_session_trace_socket);
PHP_FUNCTION(apd_set_session);

PHP_FUNCTION(apd_breakpoint);
PHP_FUNCTION(apd_continue);
PHP_FUNCTION(apd_echo);
PHP_FUNCTION(apd_get_active_symbols);
PHP_FUNCTION(apd_get_function_table);

PHP_MINIT_FUNCTION(apd);
PHP_RINIT_FUNCTION(apd);
PHP_RSHUTDOWN_FUNCTION(apd);
PHP_MINFO_FUNCTION(apd);

void printUnsortedSummary(struct timeval TSRMLS_DC);

extern zend_module_entry apd_module_entry;
#define apd_module_ptr &apd_module_entry

#define APD_VERSION "0.1"

#define FUNCTION_TRACE 1
#define ARGS_TRACE 2
#define ASSIGNMENT_TRACE 4
#define STATEMENT_TRACE 8
#define MEMORY_TRACE 16
#define TIMING_TRACE 32
#define SUMMARY_TRACE 64
#define ERROR_TRACE 128
#define PROF_TRACE 256

ZEND_BEGIN_MODULE_GLOBALS(apd)
	void* stack;
	HashTable* summary;
	HashTable* file_summary;
	char* dumpdir; /* directory for dumping seesion traces to */
	FILE* dump_file; /* FILE for dumping session traces to */
	FILE* pprof_file; /* File for profiling output */
	int dump_sock_id; /* Socket for dumping data to */
	struct timeval req_begin;  /* Time the request began */
	struct timeval lasttime;  /* Last time recorded */
	clock_t firstclock;  /* Last time recorded */
	clock_t lastclock;  /* Last time recorded */
	struct tms firsttms;  /* Last time recorded */
	struct tms lasttms;  /* Last time recorded */
	int index;                /* current index of functions for pprof tracing */
	int file_index;                /* current index of functions for pprof tracing */
	long bitmask;              /* Bitmask for determining what gets logged */
	long pproftrace;           /* Flag for whether we are doing profiling */
	void* last_mem_header;		/* tail of persistent zend_mem_header list */
	void* last_pmem_header;		/* tail of persistent zend_mem_header list */
	int interactive_mode;     /* is interactive mode on */
	int ignore_interactive;   /* ignore interactive mode flag for executing php from the debugger*/
	int allocated_memory;
ZEND_END_MODULE_GLOBALS(apd)

#ifdef ZTS
#define APD_GLOBALS(v) TSRMG(apd_globals_id, zend_apd_globals *, v)
#else
#define APD_GLOBALS(v) (apd_globals.v)
#endif

#define phpext_apd_ptr apd_module_ptr

#endif
