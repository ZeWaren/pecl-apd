/* ==================================================================
 * APD Profiler/Debugger
 * Copyright (c) 2001 Community Connect, Inc.
 * All rights reserved.
 * ==================================================================
 * This source code is made available free and without charge subject
 * to the terms of the QPL as detailed in bundled LICENSE file, which
 * is also available at http://apc.communityconnect.com/LICENSE.
 * ==================================================================
 * Daniel Cowgill <dan@mail.communityconnect.com>
 * George Schlossnagle <george@lethargy.org>
 * ==================================================================
*/

#ifndef PHP_APD_H
#define PHP_APD_H

#ifndef PHP_WIN32
#include "config.h"
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
#include <sys/time.h>
#include <unistd.h>
#endif

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

ZEND_BEGIN_MODULE_GLOBALS(apd)
	void* stack;
	HashTable* summary;
	char* dumpdir; /* directory for dumping seesion traces to */
	FILE* dump_file; /* FILE for dumping session traces to */
	struct timeval req_begin;  /* Time the request began */
	long bitmask;              /* Bitmask for determining what gets logged */
	void* last_mem_header;		/* tail of persistent zend_mem_header list */
	void* last_pmem_header;		/* tail of persistent zend_mem_header list */
	int allocated_memory;
ZEND_END_MODULE_GLOBALS(apd)

#ifdef ZTS
#define APD_GLOBALS(v) TSRMG(apd_globals_id, zend_apd_globals *, v)
#else
#define APD_GLOBALS(v) (apd_globals.v)
#endif

#define phpext_apd_ptr apd_module_ptr

#endif
