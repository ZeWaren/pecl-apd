/* 
   +----------------------------------------------------------------------+
   | APD Profiler & Debugger                                              |
   +----------------------------------------------------------------------+
   | Copyright (c) 2001-2003 Community Connect Inc.                       |
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
   |          Sterling Hughes <sterling@php.net>                          |
   +----------------------------------------------------------------------+
*/

#ifndef PHP_APD_H
#define PHP_APD_H

#include "php.h"

#if PHP_WIN32
#include "config.w32.h"
#else
#include "php_config.h"
#endif

#include "php_ini.h"
#include "php_globals.h"
#include "ext/standard/php_string.h"
#include "apd_array.h"

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
#include <sys/resource.h>
#include <unistd.h>
#else /* windows */
/* these are from ext/socket -- probably worth just copying the files into apd? */
# include "win32compat.h"
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
PHP_FUNCTION(override_function);
PHP_FUNCTION(rename_function);
PHP_FUNCTION(apd_set_pprof_trace);
PHP_FUNCTION(apd_set_browser_trace);
PHP_FUNCTION(apd_set_session_trace_socket);

PHP_FUNCTION(apd_breakpoint);
PHP_FUNCTION(apd_continue);
PHP_FUNCTION(apd_echo);

PHP_MINIT_FUNCTION(apd);
PHP_RINIT_FUNCTION(apd);
PHP_RSHUTDOWN_FUNCTION(apd);
PHP_MINFO_FUNCTION(apd);

void printUnsortedSummary(struct timeval TSRMLS_DC);

extern zend_module_entry apd_module_entry;
#define apd_module_ptr &apd_module_entry

#define APD_VERSION "0.9"

#define FUNCTION_TRACE 1
#define ARGS_TRACE 2
#define ASSIGNMENT_TRACE 4
#define STATEMENT_TRACE 8
#define MEMORY_TRACE 16
#define TIMING_TRACE 32
#define SUMMARY_TRACE 64
#define ERROR_TRACE 128
#define PROF_TRACE 256

typedef struct {
	int id;
	char *filename;
} apd_file_entry_t;

typedef struct _apd_fcall {
	int   line;
	int   file;
	long  usertime;
	long  systemtime;
	long  realtime;
	long  cumulative;
	int   memory;
	int   calls;
	void *entry;

	struct _apd_fcall *next;
	struct _apd_fcall *prev;
} apd_fcall_t;

typedef struct _apd_coverage {
	apd_fcall_t *head;
	apd_fcall_t *tail;

	int size;
} apd_coverage_t;

typedef struct {
	apd_coverage_t coverage;
	char *name;
	int   id;
	int   type;
} apd_function_entry_t;


typedef struct {
	apd_array_t functions;
	apd_array_t files;
	zend_llist  call_list;
	char *caller;
} apd_summary_t;

void apd_summary_output_header(void);
void apd_summary_output_file_reference(int, const char *);
void apd_summary_output_elapsed_time(int, int, int, int, int);
void apd_summary_output_declare_function(int, const char *, int);
void apd_summary_output_enter_function(int, int, int);
void apd_summary_output_exit_function(int, int);
void apd_summary_output_footer(void);


typedef void (*apd_output_header_func_t)(void);
typedef void (*apd_output_file_reference_func_t)(int, const char *);
typedef void (*apd_output_elapsed_time_func_t)(int, int, int, int, int);
typedef void (*apd_output_declare_function_func_t)(int, const char *, int);
typedef void (*apd_output_function_enter_func_t)(int, int, int); 
typedef void (*apd_output_function_exit_func_t)(int, int);
typedef void (*apd_output_footer_func_t)(void);

typedef struct {
	apd_output_header_func_t header;
	apd_output_footer_func_t footer;
	apd_output_file_reference_func_t file_reference;
	apd_output_elapsed_time_func_t  elapsed_time;
	apd_output_declare_function_func_t declare_function;
	apd_output_function_enter_func_t enter_function;
	apd_output_function_exit_func_t exit_function;
} apd_output_handlers;

ZEND_BEGIN_MODULE_GLOBALS(apd)
	int counter;
	void* stack;
	HashTable* function_summary;
	HashTable* file_summary;
	char* dumpdir; /* directory for dumping seesion traces to */
	FILE* dump_file; /* FILE for dumping session traces to */
	FILE* pprof_file; /* File for profiling output */
	int dump_sock_id; /* Socket for dumping data to */
	struct timeval first_clock;
	struct timeval last_clock;
	struct rusage first_ru;
	struct rusage last_ru;
	int function_index;                /* current index of functions for pprof tracing */
	int file_index;                /* current index of functions for pprof tracing */
	int current_file_index;
	long pproftrace;           /* Flag for whether we are doing profiling */
	void* last_mem_header;		/* tail of persistent zend_mem_header list */
	void* last_pmem_header;		/* tail of persistent zend_mem_header list */
	int interactive_mode;     /* is interactive mode on */
	int ignore_interactive;   /* ignore interactive mode flag for executing php from the debugger*/
	int allocated_memory;
	apd_output_handlers output;
	apd_summary_t summary;
	int statement_tracing;
ZEND_END_MODULE_GLOBALS(apd)

PHPAPI ZEND_EXTERN_MODULE_GLOBALS(apd)

/* Declare global structure. */

#ifdef ZTS
#define APD_GLOBALS(v) TSRMG(apd_globals_id, zend_apd_globals *, v)
#else
#define APD_GLOBALS(v) (apd_globals.v)
#endif

#define phpext_apd_ptr apd_module_ptr

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
