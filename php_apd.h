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

#include "config.h"
#include "php.h"
#include "php_ini.h"
#include "php_globals.h"

#include "zend.h"
#include "zend_API.h"
#include "zend_execute.h"
#include "zend_compile.h"
#include "zend_extensions.h"

#include <sys/time.h>
#include <unistd.h>

extern zend_module_entry apd_module_entry;
#define apd_module_ptr &apd_module_entry

#define APD_VERSION "0.1"

#define FUNCTION_TRACE 1
#define ARGS_TRACE 2
#define ASSIGNMENT_TRACE 4
#define STATEMENT_TRACE 8
#define MEMORY_TRACE 16
#define TIMING_TRACE 32

ZEND_BEGIN_MODULE_GLOBALS(apd)
	void* stack;
	char* dumpdir; /* directory for dumping seesion traces to */
	FILE* dump_file; /* FILE for dumping session traces to */
	struct timeval req_begin;  /* Time the request began */
	long bitmask;              /* Bitmask for determining what gets logged */
	void* last_mem_header;		/* tail of persistent zend_mem_header list */
	void* last_pmem_header;		/* tail of persistent zend_mem_header list */
	int allocated_memory;
ZEND_END_MODULE_GLOBALS(apd)

#define APD_GLOBALS(v) (apd_globals.v)

#define phpext_apd_ptr apd_module_ptr

#endif
