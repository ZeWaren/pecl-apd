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

#include "php_apd.h"
#include "apd_lib.h"
#include "opcode.h"
#include "apd_stack.h"
#include "zend_API.h"
#include "zend_hash.h"
#include "zend_alloc.h"
#include <assert.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/stat.h>

#ifdef PHP_WIN32
#include "win32compat.h"
# define APD_IS_INVALID_SOCKET(a)   (a == INVALID_SOCKET)
#else
#include <sys/time.h>
#include <unistd.h>
# define APD_IS_INVALID_SOCKET(a)   (a < 0)
#endif

#ifndef SUN_LEN
#define SUN_LEN(su) (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

#ifndef PF_INET
#define PF_INET AF_INET
#endif

// ---------------------------------------------------------------------------
// Tracng calls to zend_compile_file
// ---------------------------------------------------------------------------
#undef TRACE_ZEND_COMPILE /* define to trace all calls to zend_compile_file */
ZEND_DLEXPORT zend_op_array* apd_compile_file(zend_file_handle* TSRMLS_DC);
ZEND_DLEXPORT zend_op_array* (*old_compile_file)(zend_file_handle* TSRMLS_DC);

/* This comes from php install tree. */
#include "ext/standard/info.h"

///* List of exported functions. */
//static unsigned char a2_arg_force_ref[] = { 2, BYREF_NONE, BYREF_FORCE };

function_entry apd_functions[] = {
	PHP_FE(apd_callstack, NULL)
	PHP_FE(apd_cluck, NULL)
	PHP_FE(apd_croak, NULL)
	PHP_FE(apd_dump_regular_resources, NULL)
	PHP_FE(apd_dump_persistent_resources, NULL)
	PHP_FE(override_function, NULL)
	PHP_FE(rename_function, NULL)
	PHP_FE(dump_function_table, NULL)
	PHP_FE(apd_set_session_trace, NULL)
	PHP_FE(apd_set_pprof_trace, NULL)
        PHP_FE(apd_set_session_trace_socket, NULL)
        PHP_FE(apd_set_session, NULL)
        PHP_FE(apd_breakpoint, NULL)
        PHP_FE(apd_continue, NULL)
        PHP_FE(apd_echo, NULL)
        PHP_FE(apd_get_active_symbols, NULL)
        PHP_FE(apd_get_function_table, NULL)
	{NULL, NULL, NULL}
};

// Declare global structure.
int print_indent;
ZEND_DECLARE_MODULE_GLOBALS(apd);

ZEND_DLEXPORT zend_op_array* apd_compile_file(zend_file_handle* zfh TSRMLS_DC)
{
	struct timeval begin;
	struct timeval end;
	zend_op_array* ret_op_array;

	if(APD_GLOBALS(bitmask) & TIMING_TRACE) {
		struct timeval elapsed;
		gettimeofday(&begin, NULL);
		timevaldiff(&begin, &APD_GLOBALS(req_begin), &elapsed);
               apd_dump_fprintf("(%3d.%06d): Entered zend_compile file.\n", elapsed.tv_sec, elapsed.tv_usec);
	}
	ret_op_array = old_compile_file(zfh TSRMLS_CC);
	if(APD_GLOBALS(bitmask) & TIMING_TRACE) {
		struct timeval elapsed;
		struct timeval diff;
		
		gettimeofday(&end, NULL);
		timevaldiff(&end, &APD_GLOBALS(req_begin), &elapsed);
		timevaldiff(&end, &begin, &diff);
                apd_dump_fprintf("(%3d.%06d): Entered zend_compile file.\n", 
                    elapsed.tv_sec, elapsed.tv_usec);
	}
	return ret_op_array;
}



// ---------------------------------------------------------------------------
// apd_dump_fprintf - Outputer - either to file or socket
// ---------------------------------------------------------------------------



void apd_dump_fprintf(const char* fmt, ...)
{
    va_list args;
    char* newStr;
    TSRMLS_FETCH();    
    va_start(args, fmt);
    newStr = apd_sprintf_real(fmt, args);
    va_end(args);
        
    if (APD_GLOBALS(dump_file) != NULL) {
        fprintf(APD_GLOBALS(dump_file), newStr);
    } else if ( APD_GLOBALS(dump_sock_id) > 0)  {
#ifndef PHP_WIN32
    write(APD_GLOBALS(dump_sock_id), newStr, strlen (newStr) + 1);
#else
    send(APD_GLOBALS(dump_sock_id), newStr, strlen (newStr) + 1, 0);
#endif
    }
        
    apd_efree(newStr);
         
}

void apd_pprof_fprintf(const char* fmt, ...)
{
    va_list args;
    char* newStr;

    TSRMLS_FETCH();
    if(!APD_GLOBALS(pproftrace)) {
         zend_error(E_ERROR, "pproftrace is unset");
        return;
    }
    va_start(args, fmt);
    newStr = apd_sprintf_real(fmt, args);
    va_end(args);
    if (APD_GLOBALS(pprof_file) != NULL) {
        fprintf(APD_GLOBALS(pprof_file), newStr);
    } 
/*
    else if ( APD_GLOBALS(prof_sock_id) > 0)  {
#ifndef PHP_WIN32
        write(APD_GLOBALS(pprof_sock_id), newStr, strlen (newStr) + 1);
#else
    send(APD_GLOBALS(pprof_sock_id), newStr, strlen (newStr) + 1, 0);
#endif
    }
*/
    apd_efree(newStr);

}


// ---------------------------------------------------------------------------------
// Interactive Mode
// ---------------------------------------------------------------------------------
 
/* interactive execution - \n continues, otherwise the data is eval'ed */

void apd_interactive () {
        char *tmpbuf=NULL,*tmp=NULL;
        char *compiled_string_description;
        zval retval;
        int length = 1024; /* the maximum command length that can be accepted! */
        int recv_len;
    
        TSRMLS_FETCH();
        if (APD_GLOBALS(interactive_mode) == 0) return;
        if (APD_GLOBALS(ignore_interactive) == 1) return;
        /* only available to sockets */
        if (APD_GLOBALS(dump_sock_id) < 1) return;
        
        /* send the prompt! */
        
        write(APD_GLOBALS(dump_sock_id), ">\n", 3);
        
        /* loop until \n is recieved */
        
        tmpbuf = apd_emalloc(length + 1);
        
        recv_len = recv(APD_GLOBALS(dump_sock_id),   tmpbuf , length, 0) ;
        if (recv_len == -1) {
                php_error(E_WARNING, "apd debugger failed to recieve data: turning off debugger");
                apd_efree(tmpbuf);
                APD_GLOBALS(interactive_mode) = 0;
                return;
        }
        tmpbuf = apd_erealloc(tmpbuf, recv_len + 1);
        tmpbuf[ recv_len ] = '\0' ;
        
        if (strcmp(tmpbuf,"\n")==0) {
                apd_efree(tmpbuf);
                return;
        }
        
        /* stop interactive debugging of it'self! */
        APD_GLOBALS(ignore_interactive) =1;
        
        /* evaluate sring */
        apd_dump_fprintf("EXEC: %s",tmpbuf);
        
        tmp = "apd_debugger";
        compiled_string_description = zend_make_compiled_string_description("Apd Debugger" TSRMLS_CC);


        if (zend_eval_string(tmpbuf, &retval, compiled_string_description TSRMLS_CC) == FAILURE) {
                efree(compiled_string_description);
                zend_error(E_ERROR, "Failure evaluating code:\n%s\n", tmpbuf);
        }

        APD_GLOBALS(ignore_interactive) =0;
        apd_efree(tmpbuf);
        
        /* call myself again! = recursive! */
        apd_interactive();
}

 

// ---------------------------------------------------------------------------
// Call Tracing (Application-specific Code)
// ---------------------------------------------------------------------------

typedef enum { CALL_ARG_VAR, CALL_ARG_REF, CALL_ARG_LITERAL } CallArgType;

typedef struct summary_t {
    int index;
	int	calls;
	int totalTime;
} summary_t;

typedef struct CallArg CallArg;
struct CallArg {
	CallArgType type;		// argument type
	char* strVal;			// string representation
	char* argName;			// argument name
};

typedef struct CallStackEntry CallStackEntry;
struct CallStackEntry {
	char* functionName;		// name of the function
	int numArgs;			// number of parameters
	CallArg* args;			// parameter list
	char *filename;			// location of call
	int lineNum;			// location of call
	struct timeval func_begin;  // time of function call
    clock_t clock_begin;        // clock ticks for func call begin
};

typedef struct apd_stack_t CallStack;

static char* boolToString(int v)
{
	return v ? apd_estrdup("true") : apd_estrdup("false");
}

static CallArg* mkCallArgVal(zend_op* curOp)
{
	CallArg* arg;

	assert(curOp->opcode == ZEND_SEND_VAL);
	arg = (CallArg*) apd_emalloc(sizeof(CallArg));
	arg->type = CALL_ARG_LITERAL;

	if (curOp->op1.op_type != IS_CONST) {
		arg->strVal = apd_estrdup("??");
		return arg;
	}

	switch (curOp->op1.u.constant.type) {
	  case IS_NULL:
		arg->strVal = apd_estrdup("(null constant)");
		break;
	  case IS_LONG:
		arg->strVal = apd_sprintf("%ld", curOp->op1.u.constant.value.lval);
		break;
	  case IS_DOUBLE:
		arg->strVal = apd_sprintf("%g", curOp->op1.u.constant.value.dval);
		break;
	  case IS_STRING:
		arg->strVal = apd_sprintf("'%s'", curOp->op1.u.constant.value.str.val);
		break;
	  case IS_BOOL:
		arg->strVal = curOp->op1.u.constant.value.lval
			? apd_estrdup("true")
			: apd_estrdup("false");
		break;
	  case IS_CONSTANT:
		arg->strVal = apd_estrdup(curOp->op1.u.constant.value.str.val);
		break;
	  default:
		arg->strVal = apd_estrdup("???");
		break;
	}

	return arg;
}

static CallArg* mkCallArgVar(zend_op* curOp, CallArgType type)
{
	CallArg* arg;
	zend_op* prvOp;
	int curSize;

	assert(curOp->opcode == ZEND_SEND_VAR || curOp->opcode == ZEND_SEND_REF);
	arg = (CallArg*) apd_emalloc(sizeof(CallArg));
	arg->type = type;
	arg->strVal = 0;
	curSize = 0;

	prvOp = curOp - 1;

	while (prvOp->opcode != ZEND_EXT_FCALL_BEGIN) {
		// Check for unrecognized opcodes.
		if (prvOp->opcode != ZEND_FETCH_R && prvOp->opcode != ZEND_FETCH_W) {
			apd_strcat(&arg->strVal, &curSize, "??");
			break;
		}

		if (prvOp->op1.op_type == IS_CONST) {
			apd_strcat(&arg->strVal, &curSize, "$");
			apd_strncat(&arg->strVal, &curSize, prvOp->op1.u.constant.value.str.val, prvOp->op1.u.constant.value.str.len);
			break;
		}
		else {
			apd_strcat(&arg->strVal, &curSize, "$");
		}
		prvOp--;
	}

	if (prvOp->opcode == ZEND_EXT_FCALL_BEGIN) {
		apd_strcat(&arg->strVal, &curSize, "??");
	}
	return arg;
}

static CallStackEntry* mkCallStackEntry(
		HashTable * func_table,
		const char* functionName,
                                        int numArgs,
										CallArg** args,
										const char *filename,
										int lineNum)
{
	CallStackEntry* entry;
	zend_function *z_func;
	TSRMLS_FETCH();

	entry = (CallStackEntry*) apd_emalloc(sizeof(CallStackEntry));
	entry->functionName = apd_estrdup(functionName);
	entry->numArgs      = numArgs;
	entry->args         = 0;
	entry->filename     = apd_estrdup(filename);
	entry->lineNum      = lineNum;
	if(APD_GLOBALS(bitmask) & TIMING_TRACE || APD_GLOBALS(pproftrace)) {
		gettimeofday(&entry->func_begin, NULL);
	} 
	else {
		entry->func_begin.tv_sec = 0;
		entry->func_begin.tv_usec = 0;
	}
    if (entry->numArgs != 0) {
        int i;

        entry->args = (CallArg*) apd_emalloc(sizeof(CallArg) * numArgs);
        for (i = 0; i < numArgs; i++) {
            entry->args[i].type = args[i]->type;
            entry->args[i].strVal = apd_estrdup(args[i]->strVal);
        }
    }

	if(zend_hash_find(func_table,(char *) functionName, strlen(functionName) + 1, (void **) &z_func) == FAILURE || z_func->type != ZEND_USER_FUNCTION)
	{
		int i;
		for(i = 0; i < numArgs; i++) {
			entry->args[i].argName = apd_estrdup("(??)");
		}
	}
	else {
		int i, j = 0;
		zend_op_array *op_array;
		op_array = &(z_func->op_array);
		for(i = 0; i< op_array->last; i++) {
			if(op_array->opcodes[i].opcode == ZEND_RECV) {
				zval *zv;
				zv = &(op_array->opcodes[i-1].op1.u.constant);
				switch(op_array->opcodes[i-1].op1.u.constant.type) {
					case IS_NULL:
						entry->args[j].argName = apd_estrdup("(null)");
						break;
					case IS_LONG:
						entry->args[j].argName = apd_sprintf("%d",zv->value.lval);
						break;
					case IS_DOUBLE:
						entry->args[j].argName = apd_sprintf("%g", zv->value.dval);
						break;
					case IS_STRING:
						entry->args[j].argName = apd_estrdup(zv->value.str.val);
						break;
					case IS_ARRAY:
						entry->args[j].argName = apd_estrdup("(array)");
						break;
					case IS_OBJECT:
						entry->args[j].argName = apd_estrdup("(object)");
						break;
					case IS_BOOL:             
						entry->args[j].argName = apd_sprintf("%d",zv->value.lval); 
						break;
					case IS_RESOURCE:         
						entry->args[j].argName = apd_estrdup("(resource)");
						break;
					case IS_CONSTANT:         
						entry->args[j].argName = apd_estrdup(zv->value.str.val);
						break;
					case IS_CONSTANT_ARRAY:   
						entry->args[j].argName = apd_estrdup("(constant array)");
						break;
					default:                
						entry->args[j].argName = apd_estrdup("(something f'd up)");
						break;
				}
				j++;
			}
		}
		while(j < numArgs) {
			entry->args[j].argName = apd_estrdup("(null)");
			j++;
		}
	}

	return entry;
}

static void freeCallStackEntry(CallStackEntry* entry)
{
	int i;

	for (i = 0; i < entry->numArgs; i++) {
		apd_efree(entry->args[i].strVal);
	}
	apd_efree(entry->args);

	apd_efree(entry->functionName);
	apd_efree(entry->filename);
	apd_efree(entry);
}

static void initializeTracer()
{
	CallStack* stack;
	TSRMLS_FETCH();
	
	stack = apd_stack_create();
	APD_GLOBALS(stack) = (void*) stack;
}

static void shutdownTracer()
{
	CallStack *stack;
	TSRMLS_FETCH();

	stack = (CallStack*) APD_GLOBALS(stack);
	apd_stack_destroy(stack);
}

static void traceFunctionEntry(
		HashTable * func_table,
		const char* functionName,
                               int numArgs,
							   CallArg** args,
							   const char *filename,
							   int lineNum)
{
	CallStack* stack;
	CallStackEntry* entry;
	struct timeval now;
	struct timeval elapsed;
	int i = 0;
	char *line;
	TSRMLS_FETCH();

	line = apd_estrdup("");

	stack = (CallStack*) APD_GLOBALS(stack);

	entry = mkCallStackEntry(func_table, functionName, numArgs, args, filename, lineNum);

	if(APD_GLOBALS(bitmask))
	{
		apd_indent(&line, 2*print_indent);
	}
    if(APD_GLOBALS(pproftrace))
    {
        struct timeval elapsed;
        struct timeval now;
        struct tms walltimes;
        clock_t clock;
        if(APD_GLOBALS(index) > 1) {
//          timevaldiff(&(entry->func_begin), &APD_GLOBALS(lasttime), &elapsed);
//          APD_GLOBALS(lasttime) = entry->func_begin;
        } else {
//          gettimeofday(&now, NULL);
//          timevaldiff(&now, &APD_GLOBALS(lasttime), &elapsed);
//          APD_GLOBALS(lasttime) = now;
        }
        clock = times(&walltimes);
//      APD_GLOBALS(lasttime) = entry->func_begin;
        if ( (walltimes.tms_utime - APD_GLOBALS(lasttms).tms_utime) ||
             (walltimes.tms_stime - APD_GLOBALS(lasttms).tms_stime) ||
             (clock - APD_GLOBALS(lastclock)) )
        {
            apd_pprof_fprintf("@ %d %d %d\n", 
                walltimes.tms_utime - APD_GLOBALS(lasttms).tms_utime, 
                walltimes.tms_stime - APD_GLOBALS(lasttms).tms_stime, 
                clock - APD_GLOBALS(lastclock));
        }
        APD_GLOBALS(lasttms) = walltimes;
        APD_GLOBALS(lastclock) = clock;
    }
    if(APD_GLOBALS(bitmask) & TIMING_TRACE)
    {
  		char *tmp;
		int curSize;
		timevaldiff(&(entry->func_begin), &APD_GLOBALS(req_begin), &elapsed);
		tmp = apd_sprintf("(%3d.%06d): ", 
			elapsed.tv_sec, elapsed.tv_usec);
		curSize = strlen(tmp);
		apd_strcat(&tmp, &curSize, line);
		apd_efree(line);
		line = tmp;
    }
	if(APD_GLOBALS(bitmask) & FUNCTION_TRACE) {
		char *tmp;
		int curSize;
		tmp =apd_sprintf("%s() %s:%d\n", entry->functionName,
				entry->filename, entry->lineNum);
		curSize = strlen(line);
		apd_strcat(&line, &curSize, tmp);
		apd_efree(tmp);
	}
        if(APD_GLOBALS(bitmask)) {
                apd_dump_fprintf( "%s", line);
		if(line) {
			apd_efree(line);
		}
	}
	if(numArgs && (APD_GLOBALS(bitmask) & ARGS_TRACE)) {
		int j, k = 0;
		for(j = 0; j < numArgs; j++) {
			char* argline = NULL;
			char *tmp;
			int curSize;
			apd_indent(&argline, 2*(print_indent + 7));
			tmp = apd_sprintf("++ argv[%d] $%s = %s\n", j, 
				entry->args[j].argName, entry->args[j].strVal);
			curSize = strlen(argline) ;
			apd_strcat(&argline, &curSize, tmp);
                        apd_dump_fprintf("%s", argline);
			apd_efree(argline);
		}
	}
    if(APD_GLOBALS(pproftrace)) {
        summary_t *summaryStats;
        if ( zend_hash_find(APD_GLOBALS(summary), entry->functionName, strlen(entry->functionName) + 1, (void *) &summaryStats) == SUCCESS )
        {
            apd_pprof_fprintf("+ %d\n", summaryStats->index);
        }
        else {
            summaryStats = (summary_t *) emalloc(sizeof(summary_t));
            summaryStats->calls = 1;
            summaryStats->index = ++APD_GLOBALS(index);
            summaryStats->totalTime = 0;
            zend_hash_add(APD_GLOBALS(summary), entry->functionName, strlen(entry->functionName) + 1, summaryStats, sizeof(summary_t), NULL);
            apd_pprof_fprintf("& %d %s\n", summaryStats->index, entry->functionName);
            apd_pprof_fprintf("+ %d\n", summaryStats->index);
        }
    }
	if(APD_GLOBALS(bitmask) & MEMORY_TRACE) {
		int new_allocated_memory;
		int new_allocated_pmemory;
		char* memline;

		char *tmp;
		int curSize;

		zend_mem_header *zmh, *zpmh;

		memline = NULL;
		new_allocated_memory = 0;
		new_allocated_pmemory = 0;
#if MEMORY_LIMIT
		apd_indent(&memline, 2*(print_indent + 7));
		apd_sprintcatf(&memline, "Memory usage: %d\n", AG(allocated_memory)); // FIXME
                apd_dump_fprintf( "%s", memline);
		apd_efree(memline);
		memline = NULL;
#endif
/*  This breaks under 4.0.8-dev and didn't work well under 4.0.6

		apd_indent(&memline, 2*(print_indent + 7));
		if(APD_GLOBALS(last_mem_header) == NULL) {
			APD_GLOBALS(last_mem_header) = AG(head);
		}
		zmh = APD_GLOBALS(last_mem_header);
		while(zmh != NULL) {
			new_allocated_memory += zmh->size;
			APD_GLOBALS(allocated_memory) += zmh->size;
			zmh = zmh->pNext;
		}
		if(APD_GLOBALS(last_pmem_header) == NULL) {
			APD_GLOBALS(last_pmem_header) = AG(phead);
		}
		zpmh = APD_GLOBALS(last_pmem_header);
		while(zpmh != NULL) {
			new_allocated_pmemory += zpmh->size;
			APD_GLOBALS(allocated_memory) += zpmh->size;
			zpmh = zpmh->pNext;
		}
		if(AG(head)) {
			APD_GLOBALS(last_mem_header) = AG(head)->pLast;
		}
		if(AG(phead)) {
			APD_GLOBALS(last_pmem_header) = AG(phead)->pLast;
		}
		tmp = apd_sprintf("Memory: new regular(%d) new persistent (%d) total(%d)\n", new_allocated_memory, new_allocated_pmemory, APD_GLOBALS(allocated_memory));
		curSize = strlen(memline);
		apd_strcat(&memline, &curSize, tmp);
		apd_efree(tmp);
		fprintf(APD_GLOBALS(dump_file), "%s", memline);
		apd_efree(memline);
*/
	}
	print_indent++;
	apd_stack_push(stack, entry);
}

static void traceFunctionExit()
{
	CallStack* stack;
	CallStackEntry* entry;
	char* line = NULL;
	char* tmp;
	struct timeval now;
	struct timeval elapsed;
	struct timeval profelapsed;
	struct timeval diff;
	TSRMLS_FETCH();

	stack = (CallStack*) APD_GLOBALS(stack);
 
 	line = apd_estrdup("");
	if(print_indent) {	
 		print_indent--;
	}
	entry = (CallStackEntry *) apd_stack_pop(stack);
	if(APD_GLOBALS(bitmask)) 
	{
		apd_indent(&line, 2*print_indent);
	}
    if( APD_GLOBALS(pproftrace)) 
    {
        struct tms walltimes;
        clock_t clock;

        clock = times(&walltimes);

        if (APD_GLOBALS(index) > 1) {
            APD_GLOBALS(lasttime) = entry->func_begin;
            if( (walltimes.tms_utime - APD_GLOBALS(lasttms).tms_utime) ||
                (walltimes.tms_stime - APD_GLOBALS(lasttms).tms_stime) ||
                (clock - APD_GLOBALS(lastclock)))
            {
                apd_pprof_fprintf("@ %d %d %d\n",
                    walltimes.tms_utime - APD_GLOBALS(lasttms).tms_utime,
                    walltimes.tms_stime - APD_GLOBALS(lasttms).tms_stime,
                    clock - APD_GLOBALS(lastclock));
            }
        }
        APD_GLOBALS(lasttms) = walltimes;
        APD_GLOBALS(lastclock) = clock;
    }
	if(APD_GLOBALS(bitmask) & TIMING_TRACE)
	{
        int curSize;

	    gettimeofday(&now, NULL);
        timevaldiff(&now, &APD_GLOBALS(req_begin), &elapsed);
        tmp = apd_sprintf("(%3d.%06d): ", elapsed.tv_sec, elapsed.tv_usec);
	    curSize = strlen(tmp);
	    apd_strcat(&tmp, &curSize, line);
	    apd_efree(line);
	    line = tmp;
  }
    if(APD_GLOBALS(pproftrace))
    {
        summary_t *summaryStats;
        if ( zend_hash_find(APD_GLOBALS(summary), entry->functionName, 
                                                  strlen(entry->functionName) + 1, 
                                                  (void *) &summaryStats) == SUCCESS )
        {
            apd_pprof_fprintf("- %d\n", summaryStats->index);
        } else {
            summaryStats = (summary_t *) emalloc(sizeof(summary_t));
            summaryStats->calls = 1;
            summaryStats->index = ++APD_GLOBALS(index);
            summaryStats->totalTime = 0;
            zend_hash_add(APD_GLOBALS(summary), entry->functionName, strlen(entry->functionName) +
 1, summaryStats, sizeof(summary_t), NULL);
            apd_pprof_fprintf("- %d\n", summaryStats->index);
        }
    }
  if(APD_GLOBALS(bitmask) & FUNCTION_TRACE)
  {
  	char *tmp;
	int curSize;
	if(entry->func_begin.tv_sec) {
		timevaldiff(&now, &(entry->func_begin), &diff);
	}
	else {
		timevaldiff(&now, &APD_GLOBALS(req_begin), &diff);
	}
	if(APD_GLOBALS(bitmask) & SUMMARY_TRACE || APD_GLOBALS(pproftrace)) {
		summary_t *summaryStats;
		if ( zend_hash_find(APD_GLOBALS(summary), entry->functionName, strlen(entry->functionName) + 1, (void *) &summaryStats) == SUCCESS )
		{
			summaryStats->calls += 1;
			summaryStats->totalTime += (diff.tv_sec * 100000 + diff.tv_usec);
		}
		else {
			summaryStats = (summary_t *) emalloc(sizeof(summary_t));
			summaryStats->calls = 1;
            summaryStats->index = ++APD_GLOBALS(index);
			summaryStats->totalTime = (diff.tv_sec * 100000 + diff.tv_usec);
			zend_hash_add(APD_GLOBALS(summary), entry->functionName, strlen(entry->functionName) + 1, summaryStats, sizeof(summary_t), NULL);
		}
	}
    tmp = apd_sprintf("%s() at %s:%d returned.  Elapsed (%d.%06d)\n", 
                                entry->functionName, 
                                zend_get_executed_filename(TSRMLS_C),
                                zend_get_executed_lineno(TSRMLS_C),
                                diff.tv_sec, diff.tv_usec);
	curSize = strlen(line) ;
	apd_strcat(&line, &curSize, tmp);
	apd_efree(tmp);
  }
  if(APD_GLOBALS(bitmask)) {
        apd_dump_fprintf( "%s", line);
	apd_efree(line);
  }
}

// --------------------------------------------------------------------------
// Error Tracing
// --------------------------------------------------------------------------

void (*old_zend_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);

static void apd_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
  char buffer[1024];
  char *error_type_str;
  char *tmp;
  char *line = NULL;
    int curSize;

	TSRMLS_FETCH();

  if(APD_GLOBALS(bitmask) & ERROR_TRACE) {
    switch(type) {
      case E_ERROR:
      case E_CORE_ERROR:
      case E_COMPILE_ERROR:
      case E_USER_ERROR:
        error_type_str = "Fatal Error";
        break;
      case E_WARNING:
      case E_CORE_WARNING:
      case E_COMPILE_WARNING:
      case E_USER_WARNING:
        error_type_str = "Warning";
        break;
      case E_PARSE:
        error_type_str = "Parse error";
        break;
      case E_NOTICE:
        error_type_str = "Warning";
        break;
      case E_USER_NOTICE:
        error_type_str = "Notice";
        break;
      default:
        error_type_str = "Unknown error";
        break;
    }
    vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    line = apd_estrdup("");
    apd_indent(&line, 2*print_indent);
    if(APD_GLOBALS(bitmask) & TIMING_TRACE)
    {
      struct timeval now;
      struct timeval elapsed;

      gettimeofday(&now, NULL);
      timevaldiff(&now, &APD_GLOBALS(req_begin), &elapsed);
      tmp = apd_sprintf("(%3d.%06d): ", elapsed.tv_sec, elapsed.tv_usec);
      curSize = strlen(tmp);
      apd_strcat(&tmp, &curSize, line);
      apd_efree(line);
      line = tmp;
    }
    tmp = apd_sprintf("%s at %s:%s() line %d error_string \"%s\"\n",
            error_type_str, error_filename, 
            EG(active_op_array)->function_name?EG(active_op_array)->function_name:"main",
			error_lineno, buffer);
    curSize = strlen(line) ;
    apd_strcat(&line, &curSize, tmp);
    apd_efree(tmp);
    if(APD_GLOBALS(bitmask)) {
      apd_dump_fprintf( "%s", line);
      apd_efree(line);
    }
  }
  old_zend_error_cb(type, error_filename, error_lineno, format, args);
}

// ---------------------------------------------------------------------------
// Module Entry
// ---------------------------------------------------------------------------

zend_module_entry apd_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
	"APD",
	apd_functions,
	PHP_MINIT(apd),
	NULL,
	PHP_RINIT(apd),
	PHP_RSHUTDOWN(apd),
	PHP_MINFO(apd),
	NULL,
	NULL,
#if ZEND_MODULE_API_NO >= 20010901
    NO_VERSION_YET,          /* extension version number (string) */
#endif
	STANDARD_MODULE_PROPERTIES_EX
};
ZEND_DECLARE_MODULE_GLOBALS(apd);

#if COMPILE_DL_APD
ZEND_GET_MODULE(apd)
#endif

// ---------------------------------------------------------------------------
// PHP Configuration Functions
// ---------------------------------------------------------------------------

static PHP_INI_MH(set_dumpdir)
{
    struct stat sb;
    if (new_value == NULL || stat(new_value, &sb)) {
        APD_GLOBALS(dumpdir) = NULL;
    }
    else {
        APD_GLOBALS(dumpdir) = new_value;
    }
    return SUCCESS;
}

PHP_INI_BEGIN()
    PHP_INI_ENTRY("apd.dumpdir",    NULL,   PHP_INI_ALL, set_dumpdir)
	STD_PHP_INI_ENTRY("apd.bitmask", "0", PHP_INI_ALL, OnUpdateInt, 
		bitmask, zend_apd_globals, apd_globals)
PHP_INI_END()


// ---------------------------------------------------------------------------
// Module Startup and Shutdown Function Definitions
// ---------------------------------------------------------------------------

static void php_apd_init_globals(zend_apd_globals *apd_globals) {
    struct tms walltimes;
	memset(apd_globals, 0, sizeof(zend_apd_globals));
    
}

PHP_MINIT_FUNCTION(apd)
{
	ZEND_INIT_MODULE_GLOBALS(apd, php_apd_init_globals, NULL);
	REGISTER_INI_ENTRIES();
        REGISTER_LONG_CONSTANT("APD_AF_UNIX",      AF_UNIX,        CONST_CS | CONST_PERSISTENT);
        REGISTER_LONG_CONSTANT("APD_AF_INET",       AF_INET,        CONST_CS | CONST_PERSISTENT);
#ifdef TRACE_ZEND_COMPILE /* we can trace the time to compile things. */
	gettimeofday(&APD_GLOBALS(req_begin), NULL);
//	gettimeofday(&APD_GLOBALS(last_call), NULL);
	old_compile_file = zend_compile_file;
	zend_compile_file = apd_compile_file;
#endif
	old_zend_error_cb = zend_error_cb;
	zend_error_cb = apd_error_cb;
	return SUCCESS;
}

PHP_RINIT_FUNCTION(apd)
{
	gettimeofday(&APD_GLOBALS(req_begin), NULL);
//	gettimeofday(&APD_GLOBALS(last_call), NULL);
//	APD_GLOBALS(last_mem_header) = AG(head)->pLast;
//	APD_GLOBALS(last_pmem_header) = AG(phead)->pLast;
	APD_GLOBALS(dump_file) = stderr;
    APD_GLOBALS(dump_sock_id) = 0;
	APD_GLOBALS(bitmask) = 0;
	APD_GLOBALS(summary) = (HashTable*) emalloc(sizeof(HashTable));
    APD_GLOBALS(interactive_mode) = 0;
    APD_GLOBALS(ignore_interactive) = 0;  
    APD_GLOBALS(lastclock) = times(&APD_GLOBALS(lasttms));
    APD_GLOBALS(firstclock) = APD_GLOBALS(lastclock);
    gettimeofday(&APD_GLOBALS(lasttime), NULL);
	zend_hash_init(APD_GLOBALS(summary), 0, NULL, NULL, 0);
	initializeTracer();
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apd)
{
	if(APD_GLOBALS(bitmask))
	{
		struct timeval req_end, elapsed;
		time_t starttime;

		starttime = time(0);
		gettimeofday(&req_end, NULL);
		timevaldiff(&req_end, &APD_GLOBALS(req_begin), &elapsed);
		apd_dump_fprintf("(%3d.%06d): RSHUTDOWN called - end of trace\n", elapsed.tv_sec, elapsed.tv_usec);
    	        apd_dump_fprintf("---------------------------------------------------------------------------\n");
               apd_dump_fprintf("Process Pid (%d)\n", getpid());
		if(APD_GLOBALS(bitmask) & SUMMARY_TRACE) {
			printUnsortedSummary(elapsed TSRMLS_CC);
		}
		apd_dump_fprintf("---------------------------------------------------------------------------\n");
		apd_dump_fprintf("Trace Ended at %s", ctime(&starttime));
		apd_dump_fprintf("---------------------------------------------------------------------------\n");
	}
	shutdownTracer();
	if (APD_GLOBALS(dump_file)) {
		fclose(APD_GLOBALS(dump_file));
	}
    if(APD_GLOBALS(pprof_file)) {
        struct tms endtp;
        clock_t endclock;
        endclock = times(&endtp);
        apd_pprof_fprintf("END_TRACE\n");
        apd_pprof_fprintf("total_user=%d\ntotal_sys=%d\ntotal_wall=%d\n",
            endtp.tms_utime,
            endtp.tms_stime,
            endclock - APD_GLOBALS(firstclock));
        apd_pprof_fprintf("END_FOOTER\n");
        fclose(APD_GLOBALS(pprof_file));
    }
    if (APD_GLOBALS(dump_sock_id)) {
        close(APD_GLOBALS(dump_sock_id));
        /* bit academic - but may as well */
        APD_GLOBALS(dump_sock_id)=0;
    }
	zend_hash_destroy(APD_GLOBALS(summary));
	efree(APD_GLOBALS(summary));
	return SUCCESS;
}

PHP_MINFO_FUNCTION(apd)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Advanced PHP Debugger (APD)", "Enabled");
	php_info_print_table_row(2, "APD Version", APD_VERSION);
	php_info_print_table_end();
}


// ---------------------------------------------------------------------------
// PHP Extension Functions
// ---------------------------------------------------------------------------

PHP_FUNCTION(apd_callstack)
{
	CallStack* stack;
	void** elements;
	int numElements;
	int i;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}

	stack = (CallStack*) APD_GLOBALS(stack);
	elements = apd_stack_toarray(stack);
	numElements = apd_stack_getsize(stack);

	if (array_init(return_value) != SUCCESS) {
		assert(0);	// XXX: handle failure
	}

	for (i = numElements-2; i >= 0; i--) {
		CallStackEntry* stackEntry;
		zval* phpEntry;
		zval* args;
		int j;
		
		stackEntry = (CallStackEntry*) elements[i];

		MAKE_STD_ZVAL(phpEntry);
		if (array_init(phpEntry) != SUCCESS) {
			assert(0);	// XXX: handle failure
		}

		add_index_string(phpEntry, 0, stackEntry->functionName, 1);
		add_index_string(phpEntry, 1, stackEntry->filename, 1);
		add_index_long  (phpEntry, 2, stackEntry->lineNum);

		MAKE_STD_ZVAL(args);
		if (array_init(args) != SUCCESS) {
			assert(0);	// XXX: handle failure
		}

		for (j = 0; j < stackEntry->numArgs; j++) {
			add_index_string(args, j, stackEntry->args[j].strVal, 1);
		}
		zend_hash_index_update(Z_ARRVAL_P(phpEntry), 3,
			&args, sizeof(zval*), NULL);

		zend_hash_index_update(Z_ARRVAL_P(return_value), numElements - i - 2,
			&phpEntry, sizeof(zval*), NULL);
			
//		add_index_string(return_value, i, stackEntry->functionName, 1);
	}

//	RETURN_TRUE;
}

PHP_FUNCTION(apd_cluck)
{
    CallStack* stack;
    void** elements;
    int numElements;
    int i;
	char *errstr = NULL, *delimiter = NULL;
	int errstr_len = 0, delimiter_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ss", &errstr, &errstr_len, &delimiter, &delimiter_len) == FAILURE) {
		return;
	}
	zend_printf("%s at %s line %d%s", 
				errstr ? errstr : "clucked",
				zend_get_executed_filename(TSRMLS_C), 
				zend_get_executed_lineno(TSRMLS_C),
				delimiter ? delimiter : "<BR />\n");

    stack = (CallStack*) APD_GLOBALS(stack);
    elements = apd_stack_toarray(stack);
    numElements = apd_stack_getsize(stack);


    for (i = numElements-2; i >= 0; i--) {
        CallStackEntry* stackEntry;
        zval* phpEntry;
        zval* args;
        int j;

        stackEntry = (CallStackEntry*) elements[i];

        zend_printf("%s(", stackEntry->functionName);
        for (j = 0; j < stackEntry->numArgs; j++) {
            if (j < stackEntry->numArgs - 1) {
                zend_printf("%s,", stackEntry->args[j].strVal);
            }
            else {
                zend_printf("%s", stackEntry->args[j].strVal);
            }
        }
        zend_printf(") called at %s line %d%s",
                    stackEntry->filename,
                    stackEntry->lineNum,
					delimiter ? delimiter : "<BR />\n");
    }

//  RETURN_TRUE;
}

PHP_FUNCTION(apd_croak)
{
    CallStack* stack;
    void** elements;
    int numElements;
    int i;
	char *errstr = NULL, *delimiter = NULL;
	int errstr_len = 0, delimiter_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ss", &errstr, &errstr_len, &delimiter, &delimiter_len) == FAILURE) {
		return;
	}
    stack = (CallStack*) APD_GLOBALS(stack);
    elements = apd_stack_toarray(stack);
    numElements = apd_stack_getsize(stack);

	zend_printf("%s at %s line %d%s", 
				errstr ? errstr : "croaked",
				zend_get_executed_filename(TSRMLS_C), 
				zend_get_executed_lineno(TSRMLS_C),
				delimiter ? delimiter : "<BR />\n");

    for (i = numElements-2; i >= 0; i--) {
        CallStackEntry* stackEntry;
        zval* phpEntry;
        zval* args;
        int j;

        stackEntry = (CallStackEntry*) elements[i];

        zend_printf("%s(", stackEntry->functionName);
        for (j = 0; j < stackEntry->numArgs; j++) {
            if (j < stackEntry->numArgs - 1) {
                zend_printf("%s,", stackEntry->args[j].strVal);
            }
            else {
                zend_printf("%s", stackEntry->args[j].strVal);
            }
        }
        zend_printf(") called at %s line %d%s",
                    stackEntry->filename,
                    stackEntry->lineNum,
					delimiter ? delimiter : "<BR />\n");
    }
	zend_bailout();
}

PHP_FUNCTION(apd_dump_regular_resources) 
{
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	__apd_dump_regular_resources(return_value TSRMLS_CC);
}

PHP_FUNCTION(apd_dump_persistent_resources)
{  
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

    __apd_dump_persistent_resources(return_value TSRMLS_CC);
}

#define TEMP_OVRD_FUNC_NAME "__overridden__"
PHP_FUNCTION(override_function)
{
	char *eval_code,*eval_name, *temp_function_name;
	int eval_code_length, retval, temp_function_name_length;
	zval **z_function_name, **z_function_args, **z_function_code;
	
	if (ZEND_NUM_ARGS() != 3 || 
		zend_get_parameters_ex(3, &z_function_name, &z_function_args, 
		&z_function_code) == FAILURE) 
	{
		ZEND_WRONG_PARAM_COUNT();
	}

	convert_to_string_ex(z_function_name);
	convert_to_string_ex(z_function_args);
	convert_to_string_ex(z_function_code);

	eval_code_length = sizeof("function " TEMP_OVRD_FUNC_NAME) 
		+ Z_STRLEN_PP(z_function_args)
		+ 2 /* parentheses */
		+ 2 /* curlies */
		+ Z_STRLEN_PP(z_function_code);
	eval_code = (char *) emalloc(eval_code_length);
	sprintf(eval_code, "function " TEMP_OVRD_FUNC_NAME "(%s){%s}",
		Z_STRVAL_PP(z_function_args), Z_STRVAL_PP(z_function_code));
	eval_name = zend_make_compiled_string_description("runtime-created override function" TSRMLS_CC);
	retval = zend_eval_string(eval_code, NULL, eval_name TSRMLS_CC);
	efree(eval_code);
	efree(eval_name);

	if (retval == SUCCESS) {
		zend_function *func;

		if (zend_hash_find(EG(function_table), TEMP_OVRD_FUNC_NAME,
			sizeof(TEMP_OVRD_FUNC_NAME), (void **) &func) == FAILURE) 
		{
			zend_error(E_ERROR, "%s() temporary function name not present in global function_table", get_active_function_name(TSRMLS_C));
			RETURN_FALSE;
		}
		function_add_ref(func);
		zend_hash_del(EG(function_table), Z_STRVAL_PP(z_function_name),
			Z_STRLEN_PP(z_function_name) + 1);
		if(zend_hash_add(EG(function_table), Z_STRVAL_PP(z_function_name),
			Z_STRLEN_PP(z_function_name) + 1, func, sizeof(zend_function),
			NULL) == FAILURE) 
		{
			RETURN_FALSE;
		}
		RETURN_TRUE;
	}	
	else {
		RETURN_FALSE;
	}
}

PHP_FUNCTION(rename_function)
{
  zval **z_orig_fname, **z_new_fname;
  zend_function *func, *dummy_func;

  if( ZEND_NUM_ARGS() != 2 ||
    zend_get_parameters_ex(2, &z_orig_fname, &z_new_fname) == FAILURE )
  {
    ZEND_WRONG_PARAM_COUNT();
  }

  convert_to_string_ex(z_orig_fname);
  convert_to_string_ex(z_new_fname);

  if(zend_hash_find(EG(function_table), Z_STRVAL_PP(z_orig_fname),
    Z_STRLEN_PP(z_orig_fname) + 1, (void **) &func) == FAILURE)
  {
    zend_error(E_WARNING, "%s(%s, %s) failed: %s does not exist!",
	  get_active_function_name(TSRMLS_C),
      Z_STRVAL_PP(z_orig_fname),  Z_STRVAL_PP(z_new_fname),
      Z_STRVAL_PP(z_orig_fname));
    RETURN_FALSE;
  }
  if(zend_hash_find(EG(function_table), Z_STRVAL_PP(z_new_fname),
    Z_STRLEN_PP(z_new_fname) + 1, (void **) &dummy_func) == SUCCESS)
  {
    zend_error(E_WARNING, "%s(%s, %s) failed: %s already exists!",
	  get_active_function_name(TSRMLS_C),
      Z_STRVAL_PP(z_orig_fname),  Z_STRVAL_PP(z_new_fname),
      Z_STRVAL_PP(z_new_fname));
    RETURN_FALSE;
  }
  if(zend_hash_add(EG(function_table), Z_STRVAL_PP(z_new_fname),
    Z_STRLEN_PP(z_new_fname) + 1, func, sizeof(zend_function),
    NULL) == FAILURE)
  {
    zend_error(E_WARNING, "%s() failed to insert %s into EG(function_table)",
	  get_active_function_name(TSRMLS_C),
      Z_STRVAL_PP(z_new_fname));
    RETURN_FALSE;
  }
  if(zend_hash_del(EG(function_table), Z_STRVAL_PP(z_orig_fname),
    Z_STRLEN_PP(z_orig_fname) + 1) == FAILURE)
  {
    zend_error(E_WARNING, "%s() failed to remove %s from function table",
	  get_active_function_name(TSRMLS_C),
      Z_STRVAL_PP(z_orig_fname));
    zend_hash_del(EG(function_table), Z_STRVAL_PP(z_new_fname),
      Z_STRLEN_PP(z_new_fname) + 1);
    RETURN_FALSE;
  }
  RETURN_TRUE;
}

PHP_FUNCTION(dump_function_table)
{
	Bucket *p;
	HashTable *hash;
	
	hash = EG(function_table);
	p = hash->pListHead;
	while(p != NULL) {
		printf("key (%s) length (%d)\n", p->arKey, p->nKeyLength);
		p = p->pListNext;
	}
	RETURN_TRUE;
}

void apd_dump_session_start() {
        time_t starttime;
        TSRMLS_FETCH();

        starttime = time(0);
        apd_dump_fprintf("\n\nAPD - Advanced PHP Debugger Trace File\n");
        apd_dump_fprintf("---------------------------------------------------------------------------\n");
        apd_dump_fprintf("Process Pid (%d)\n", getpid());
        apd_dump_fprintf("Trace Begun at %s", ctime(&starttime));
        apd_dump_fprintf("---------------------------------------------------------------------------\n");
        apd_dump_fprintf("(  0.000000): apd_set_session_trace called at %s:%d\n", zend_get_executed_filename(TSRMLS_C), zend_get_executed_lineno(TSRMLS_C)); 
}



PHP_FUNCTION(apd_set_session_trace) 
{
	int issock =0;
	int socketd = 0;
	int path_len;
	char *dumpdir;
	char *path;
	zval **z_bitmask, **z_dumpdir;

	if(ZEND_NUM_ARGS() > 2 || ZEND_NUM_ARGS() == 0) 
	{
		ZEND_WRONG_PARAM_COUNT();
	}
	if(ZEND_NUM_ARGS() == 1)
	{
		if(zend_get_parameters_ex(1, &z_bitmask) == FAILURE)
		{
			ZEND_WRONG_PARAM_COUNT();
		}
		if(APD_GLOBALS(dumpdir)) {
			dumpdir = APD_GLOBALS(dumpdir);
		}
		else {
			zend_error(E_WARNING, "%s() no dumpdir specified",
					   get_active_function_name(TSRMLS_C));
			RETURN_FALSE;
		}
		convert_to_long(*z_bitmask);
		APD_GLOBALS(bitmask) = Z_LVAL_PP(z_bitmask);
	}
	else {
		if(zend_get_parameters_ex(2, &z_bitmask, &z_dumpdir) == FAILURE) 
		{
			ZEND_WRONG_PARAM_COUNT();
		}
		convert_to_long(*z_bitmask);
		APD_GLOBALS(bitmask) = Z_LVAL_PP(z_bitmask);

		convert_to_string_ex(z_dumpdir);
		dumpdir = Z_STRVAL_PP(z_dumpdir);
	}
	path_len = strlen(dumpdir) + 1 + strlen("apd_dump_") + 5;
	path = (char *) emalloc((path_len + 1)* sizeof(char) );
	snprintf(path, path_len + 1, "%s/apd_dump_%05d", dumpdir, getpid());
	if((APD_GLOBALS(dump_file) = fopen(path, "a")) == NULL) {
		zend_error(E_ERROR, "%s() failed to open %s for tracing", get_active_function_name(TSRMLS_C), path);
	}	
	efree(path);
        apd_dump_session_start();
}	

void apd_pprof_header() {
        apd_pprof_fprintf("#Pprof [APD] v0.9\n");
        apd_pprof_fprintf("hz=%d\n", sysconf(_SC_CLK_TCK));
        apd_pprof_fprintf("\nEND_HEADER\n");
        apd_pprof_fprintf("& 1 apd_set_session_trace\n+ 1\n");

}

PHP_FUNCTION(apd_set_pprof_trace)
{
    int issock =0;
    int socketd = 0;
    int path_len;
    char *dumpdir;
    char *path;
    zval  **z_dumpdir;
    TSRMLS_FETCH();

    if(ZEND_NUM_ARGS() > 1 )
    {
        ZEND_WRONG_PARAM_COUNT();
    }
    if(ZEND_NUM_ARGS() == 0)
    {
        if(APD_GLOBALS(dumpdir)) {
            dumpdir = APD_GLOBALS(dumpdir);
        }
        else {
            zend_error(E_WARNING, "%s() no dumpdir specified",
                       get_active_function_name(TSRMLS_C));
            RETURN_FALSE;
        }
        APD_GLOBALS(pproftrace) = 1;
    }
    else {
        if(zend_get_parameters_ex(1, &z_dumpdir) == FAILURE)
        {
            ZEND_WRONG_PARAM_COUNT();
        }
        APD_GLOBALS(pproftrace) = 1;

        convert_to_string_ex(z_dumpdir);
        dumpdir = Z_STRVAL_PP(z_dumpdir);
    }
    path_len = strlen(dumpdir) + 1 + strlen("apd_dump_") + 5;
    path = (char *) emalloc((path_len + 1)* sizeof(char) );
    snprintf(path, path_len + 1, "%s/pprof.%05d", dumpdir, getpid());
    if((APD_GLOBALS(pprof_file) = fopen(path, "a")) == NULL) {
        zend_error(E_ERROR, "%s() failed to open %s for tracing", get_active_function_name(TSRMLS_C), path);
    }  
    efree(path);
    apd_pprof_header();
}  


/* {{{ proto bool apd_set_session_trace_socket(string ip_or_filename, int domain, int port, int mask)
   Connects to a socket either unix domain or tcpip and sends data there*/

PHP_FUNCTION(apd_set_session_trace_socket) 
{
    
    char *address;
    int address_len;
    
    int sock_domain;
    int sock_port;
    int bitmask;
    int connect_result;
   
    struct sockaddr_in si;
    struct sockaddr_un su;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, 
                "slll", &address,&address_len, &sock_domain, &sock_port,&bitmask) == FAILURE) 
    {
        return;
    }
    APD_GLOBALS(bitmask) = bitmask;
    APD_GLOBALS(dump_file)=NULL;
        
    /* now for the socket stuff */
    /* is it already connected ? */
    if (APD_GLOBALS(dump_sock_id) > 0) {
        RETURN_TRUE;
    }
        
    if (sock_domain != AF_UNIX && sock_domain != AF_INET) {
        php_error(E_WARNING, "%s() invalid socket domain [%d] specified for argument 2, assuming AF_INET", get_active_function_name(TSRMLS_C), sock_domain);
       sock_domain = AF_INET;
    }
        
    APD_GLOBALS(dump_sock_id) = socket(sock_domain, SOCK_STREAM, 0);
    if (APD_IS_INVALID_SOCKET(APD_GLOBALS(dump_sock_id))) {
    /* this may not work = it assumes that 0 would be an invalid socket! */
        APD_GLOBALS(dump_sock_id)=0;
        RETURN_FALSE;
    }
    /* now connect it */
    switch(sock_domain) {
        case AF_UNIX:
        {
            su.sun_family = AF_UNIX;
            strncpy (su.sun_path, address, sizeof (su.sun_path));
            if (connect(APD_GLOBALS(dump_sock_id), (struct sockaddr *) &su, SUN_LEN(&su)) < 0)
            {
                php_error(E_WARNING, "%s() failed to connect to  [%s]", get_active_function_name(TSRMLS_C), address);
                APD_GLOBALS(dump_sock_id)=0;
                RETURN_FALSE;
            }
            break;
        }
        case AF_INET:
        {
            struct hostent *hp;
            si.sin_family = AF_INET;
            si.sin_port = htons (sock_port);
            hp = gethostbyname (address);
            if (hp == NULL) 
            {
                php_error(E_WARNING, "%s() failed to get host by name  [%s]", get_active_function_name(TSRMLS_C), address);
                APD_GLOBALS(dump_sock_id)=0;
                RETURN_FALSE;
            }
            si.sin_addr = *(struct in_addr *) hp->h_addr;
            connect_result = connect(APD_GLOBALS(dump_sock_id), (struct sockaddr *) &si, sizeof (si));
            if (0 > connect_result)
            {
                php_error(E_WARNING, "%s() failed to connect to  [%s:%d] %d", get_active_function_name(TSRMLS_C), address,sock_port,connect_result);
                APD_GLOBALS(dump_sock_id)=0;
                RETURN_FALSE;
            }
            break;
        }
    }
    apd_dump_session_start();
    RETURN_TRUE;
}

/* {{{ proto bool apd_set_session(int mask)
   Changes or sets the session trace mask*/

PHP_FUNCTION(apd_set_session) 
{
        int bitmask;
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, 
                "l", &bitmask) == FAILURE) {
                return;
        }
        APD_GLOBALS(bitmask) = bitmask;
        RETURN_TRUE;
}
/* {{{ proto void apd_breakpoint()
   Starts interactive mode*/

PHP_FUNCTION(apd_breakpoint) 
{
        int bitmask; 
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, 
                "l", &bitmask) == FAILURE) {
                return;
        } 
        APD_GLOBALS(bitmask) = bitmask;
        APD_GLOBALS(interactive_mode) = 1;
        RETURN_TRUE;
}
/* {{{ proto void apd_breakpoint()
   Stops interactive mode - normally called by interactive interpreter! */

PHP_FUNCTION(apd_continue) 
{
        int bitmask; 
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, 
                "l", &bitmask) == FAILURE) {
                return;
        } 
        APD_GLOBALS(bitmask) = bitmask;
        APD_GLOBALS(interactive_mode) = 0;
        RETURN_TRUE;
}
/* {{{ proto  apd_echo(string output)
   sends string to debugger - normally called by interactive interpreter!*/
        
PHP_FUNCTION(apd_echo) 
{
        
        char *str;
        int str_len;
         
  
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,  "s|l", &str,&str_len) == FAILURE) {
                return;
        }
        if (str_len > 0) {
//              php_error(E_WARNING, "apd echoing %s", str);            
                if ( APD_GLOBALS(dump_sock_id) > 0)  {
#ifndef PHP_WIN32
                        /* Unix bit */
                        write(APD_GLOBALS(dump_sock_id), str, str_len);
                        write(APD_GLOBALS(dump_sock_id), "\n", 2);
#else
                        /* Win32 bit */
                        send(APD_GLOBALS(dump_sock_id), str, str_len, 0);
                        send(APD_GLOBALS(dump_sock_id), "\n", 2, 0);
#endif
                }
                apd_dump_fprintf("%s\n",str);
//              efree(str);
        }
        RETURN_TRUE;

}
/* {{{ proto array apd_get_active_symbols()
   returns an array of the current active symbols (eg. local available variable) - without values! */

PHP_FUNCTION(apd_get_active_symbols)
{
        Bucket *p;
        HashTable *hash;
        zval  *data;
        int i;
        char *var_name;
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
                return;
        }
        hash = EG(active_symbol_table);
        
         
        if (array_init(return_value) == FAILURE) {
                RETURN_FALSE;
        }
        p = hash->pListHead;
        i=0;
        while(p != NULL) {
                MAKE_STD_ZVAL(data);
                var_name = apd_sprintf("%s",p->arKey);
                Z_STRVAL_P(data) = var_name;
                Z_STRLEN_P(data) = strlen(var_name) +1;
                Z_TYPE_P(data) = IS_STRING;
                zval_add_ref(&data);
                zend_hash_next_index_insert(Z_ARRVAL_P(return_value), (void *)&data, sizeof(zval *), NULL);
                
                p = p->pListNext;
                i++;
                
        }
         
         
}
/* {{{ proto array apd_get_function_table()
   returns an array of the current function table */

PHP_FUNCTION(apd_get_function_table)
{
         
        Bucket *p;
        HashTable *hash;
        zval  *data;
        int i;
        char *var_name;
        if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
                return;
        }
        hash = EG(function_table);
        
         
        if (array_init(return_value) == FAILURE) {
                RETURN_FALSE;
        }
        p = hash->pListHead;
        i=0;
        while(p != NULL) {
                MAKE_STD_ZVAL(data);
                var_name = apd_sprintf("%s",p->arKey);
                Z_STRVAL_P(data) = var_name;
                Z_STRLEN_P(data) = strlen(var_name) +1;
                Z_TYPE_P(data) = IS_STRING;
                zval_add_ref(&data);
                zend_hash_next_index_insert(Z_ARRVAL_P(return_value), (void *)&data, sizeof(zval *), NULL);
                
                p = p->pListNext;
                i++;
                
        }
        
}

// ---------------------------------------------------------------------------
// Zend Extension Functions
// ---------------------------------------------------------------------------

static const char* getOpTypeName(int optype)
{
	switch (optype) {
	  case IS_CONST:	return "IS_CONST";
	  case IS_TMP_VAR:	return "IS_TMP_VAR";
	  case IS_VAR:		return "IS_VAR";
	  case IS_UNUSED:	return "IS_UNUSED";
	  default:			return "Unkown";
	}
}

static const char* getDatatypeName(int type)
{
	switch (type) {
	  case IS_NULL:				return "IS_NULL";
	  case IS_LONG:				return "IS_LONG";
	  case IS_DOUBLE:			return "IS_DOUBLE";
	  case IS_STRING:			return "IS_STRING";
	  case IS_ARRAY:			return "IS_ARRAY";
	  case IS_OBJECT:			return "IS_OBJECT";
	  case IS_BOOL:				return "IS_BOOL";
	  case IS_RESOURCE:			return "IS_RESOURCE";
	  case IS_CONSTANT:			return "IS_CONSTANT";
	  case IS_CONSTANT_ARRAY:	return "IS_CONSTANT_ARRAY";
	  default:					assert(0);
	}
}
static void printZval(FILE* out, zval* zv)
{
	fprintf(out, "\t\t\ttype     = %s\n", getDatatypeName(zv->type));
	fprintf(out, "\t\t\tis_ref   = %d\n", zv->is_ref);
	fprintf(out, "\t\t\trefcount = %d\n", zv->refcount);
	fprintf(out, "\t\t\tvalue    = ");
	switch (zv->type) {
	  case IS_NULL:				fprintf(out, "(null)"); break;
	  case IS_LONG:				fprintf(out, "%ld", zv->value.lval); break;
	  case IS_DOUBLE:			fprintf(out, "%g", zv->value.dval); break;
	  case IS_STRING:			fprintf(out, "%s", zv->value.str.val); break;
	  case IS_ARRAY:			fprintf(out, "(array)"); break;
	  case IS_OBJECT:			fprintf(out, "(object)"); break;
	  case IS_BOOL:				fprintf(out, "%d", zv->value.lval); break;
	  case IS_RESOURCE:			fprintf(out, "(resource)"); break;
	  case IS_CONSTANT:			fprintf(out, "%s", zv->value.str.val); break;
	  case IS_CONSTANT_ARRAY:	fprintf(out, "(constant array)"); break;
	  default:					assert(0);
	}
	fprintf(out, "\n");
}

static void printZnode(FILE* out, const char* name, znode* zn)
{
	fprintf(out, "\t\t%s = %s\n", name, getOpTypeName(zn->op_type));
	switch (zn->op_type) {
	  case IS_CONST:
		printZval(out, &zn->u.constant);
		break;
	  case IS_TMP_VAR:
	  case IS_VAR:
	  case IS_UNUSED:
	  default:
		break;
	}
}

static void dumpOpArray(zend_op_array* op_array)
{
	int i;

	if (!op_array) {
		return;
	}

	fprintf(stderr, "op_array (%d opcodes)\n", op_array->last);
	fprintf(stderr, "function_name:           %s\n", op_array->function_name);
	fprintf(stderr, "last:                    %u\n", op_array->last);
	fprintf(stderr, "size:                    %u\n", op_array->size);
	fprintf(stderr, "T:                       %u\n", op_array->T);
	fprintf(stderr, "last_brk_cont:           %u\n", op_array->last_brk_cont);
	fprintf(stderr, "current_brk_cont:        %u\n", op_array->current_brk_cont);
#if 0
	fprintf(stderr, "start_op_number:         %d\n", op_array->start_op_number);
	fprintf(stderr, "end_op_number:           %d\n", op_array->end_op_number);
	fprintf(stderr, "last_executed_op_number: %d\n", op_array->last_executed_op_number);
#endif
	for (i = 0; i < op_array->last; i++) {
		printZnode(stderr, "result", &op_array->opcodes[i].result);
		printZnode(stderr, "op1   ", &op_array->opcodes[i].op1);
		printZnode(stderr, "op2   ", &op_array->opcodes[i].op2);
	}
}

ZEND_DLEXPORT void fcallBegin(zend_op_array *op_array)
{
	zend_op* curOpCode;		// working opcode ptr
	zend_op* endOpCode;		// one past the last opcode
	apd_stack_t* argStack;
	char* functionName;
	char fname_buffer[1024];
	HashTable *func_table;
	TSRMLS_FETCH();

	func_table = EG(function_table);

	curOpCode = *EG(opline_ptr);
	endOpCode = op_array->opcodes + op_array->last + 1;
	
	assert(curOpCode->opcode == ZEND_EXT_FCALL_BEGIN);

	argStack = apd_stack_create();

	while (curOpCode < endOpCode) {
		int opcode = curOpCode->opcode;

		if (opcode == ZEND_DO_FCALL			||
			opcode == ZEND_DO_FCALL_BY_NAME	||
			opcode == ZEND_INCLUDE_OR_EVAL	||
			opcode == ZEND_EXT_FCALL_END)
		{
			break;
		}

		switch (opcode) {
		  case ZEND_SEND_VAL:
			apd_stack_push(argStack, mkCallArgVal(curOpCode));
			break;
		  case ZEND_SEND_VAR:
			apd_stack_push(argStack, mkCallArgVar(curOpCode, CALL_ARG_VAR));
			break;
		  case ZEND_SEND_REF:
			apd_stack_push(argStack, mkCallArgVar(curOpCode, CALL_ARG_REF));
			break;
		}

		curOpCode++;
	}
	assert(curOpCode != endOpCode);

	switch(curOpCode->opcode)	{
		case ZEND_EXT_FCALL_END:
			functionName = apd_estrdup("func???");
			break;
		case ZEND_INCLUDE_OR_EVAL:
			{
				CallArg* arg;

				switch (curOpCode->op2.u.constant.value.lval) {
					case ZEND_INCLUDE:
						functionName = apd_estrdup("include");
						break;
					case ZEND_INCLUDE_ONCE:
						functionName = apd_estrdup("include_once");
						break;
					case ZEND_REQUIRE:
						functionName = apd_estrdup("require");
						break;
					case ZEND_REQUIRE_ONCE:
						functionName = apd_estrdup("require_once");
						break;
					case ZEND_EVAL:
						functionName = apd_estrdup("eval");
						break;
					default:
						functionName = apd_estrdup("[unknown]");
						break;
				}

				arg = (CallArg*) apd_emalloc(sizeof(CallArg));
				arg->type = CALL_ARG_LITERAL;
				arg->strVal = apd_copystr(curOpCode->op1.u.constant.value.str.val,curOpCode->op1.u.constant.value.str.len);
				apd_stack_push(argStack, arg);
			}
			break;
		case ZEND_DO_FCALL:
			switch(curOpCode->op1.op_type)	{
				case IS_CONST:
					if (curOpCode->op1.u.constant.type == IS_STRING) {
						functionName = apd_copystr(
								curOpCode->op1.u.constant.value.str.val,
								curOpCode->op1.u.constant.value.str.len);
					}
					break;
				case IS_VAR:  /* Does this happen? */
					if (CG(class_entry).name)	{
						sprintf(fname_buffer, "%s::%p", 
								CG(class_entry).name,
								curOpCode->op2.u.constant.value.str.val
							   );
					}
					else	{
						sprintf(fname_buffer, "<???>::%s", 
								curOpCode->op1.u.constant.value.str.val
							   );

					}
					functionName = apd_estrdup(fname_buffer);
					break;
				default:
					sprintf(fname_buffer, "<???>::%s",
						curOpCode->op1.u.constant.value.str.val
						);
					functionName = apd_estrdup(fname_buffer);
					break;
			}
			break;
		case ZEND_DO_FCALL_BY_NAME: 
			{
				zend_op* tmpOpCode;
				zval* function_name;

				tmpOpCode = curOpCode;
				while(tmpOpCode->opcode != ZEND_INIT_FCALL_BY_NAME) {
					tmpOpCode--;
				}
				switch(curOpCode->op1.op_type)  {
					case IS_CONST:
						switch(tmpOpCode->op2.op_type) {
							case IS_CONST:
								functionName = apd_copystr(
									tmpOpCode->op2.u.constant.value.str.val,
		 							tmpOpCode->op2.u.constant.value.str.len
								);
								break;
							default:  /* FIXME need better IS_VAR handling */
								functionName = apd_estrdup("null");
								break;
		
						}
						break;
					case IS_VAR:
						if (tmpOpCode->op1.op_type == IS_CONST)   {
							switch(tmpOpCode->op2.op_type) {
								case IS_CONST:
									sprintf(fname_buffer, "%s::%s",
										tmpOpCode->op1.u.constant.value.str.val,
										tmpOpCode->op2.u.constant.value.str.val
									);
									break;
								default:
									sprintf(fname_buffer, "%s::<???>",
										tmpOpCode->op1.u.constant.value.str.val
									);
									break;
							}
						}
						else if(CG(class_entry).name) {
							switch(tmpOpCode->op2.op_type) {
								case IS_CONST:
									sprintf(fname_buffer, "%s::%s",
										CG(class_entry).name,
										tmpOpCode->op2.u.constant.value.str.val
									);
									break;
								default:
									sprintf(fname_buffer, "%s::<???>",
										CG(class_entry).name
									);
									break;
							}
						}
						else {
							switch(tmpOpCode->op2.op_type) {
								case IS_CONST:
									sprintf(fname_buffer, "<???>::%s",
										tmpOpCode->op2.u.constant.value.str.val
									);
									break;
								default:
									sprintf(fname_buffer, "<???>::<???>");
									break;
							}
						}
						functionName = apd_estrdup(fname_buffer);
				}
								
			}
			break;
		default:
			fprintf(stderr, "Unexpected error %s:%d\n", __FILE__, __LINE__);
			assert(0);
			break;
			
	}

	traceFunctionEntry(func_table, functionName, apd_stack_getsize(argStack),
		(CallArg**) apd_stack_toarray(argStack), op_array->filename,
		curOpCode->lineno);

	apd_efree(functionName);

	apd_stack_apply(argStack, apd_efree);
	apd_stack_destroy(argStack);

//dumpOpArray(op_array);
}

void printUnsortedSummary(struct timeval elapsed TSRMLS_DC)
{
	Bucket *p;
	summary_t* summary;
	uint i;

	apd_dump_fprintf("%% time     usecs  usecs/call     calls    function\n");
	apd_dump_fprintf("-----      -----  ----------     -----    --------\n");
	p = APD_GLOBALS(summary)->pListHead;
	while(p != NULL) {
		summary = (summary_t*) p->pData;
		apd_dump_fprintf("%3.2f %10d  %10d  %8d    %s\n", ((float)summary->totalTime)/((float)(100000*elapsed.tv_sec + elapsed.tv_usec)) * 100.0, summary->totalTime, summary->totalTime/summary->calls, summary->calls, p->arKey);
		p = p->pListNext;
	}
}

ZEND_DLEXPORT void fcallEnd(zend_op_array *op_array)
{
	traceFunctionExit();
        apd_interactive();
}


ZEND_DLEXPORT void onStatement(zend_op_array *op_array)
{
        TSRMLS_FETCH();
        if (APD_GLOBALS(ignore_interactive) == 1) return;
        if(APD_GLOBALS(bitmask) & STATEMENT_TRACE) {
            apd_dump_fprintf("statement: %s:%d\n",                   
                zend_get_executed_filename(TSRMLS_C),
                zend_get_executed_lineno(TSRMLS_C));
            apd_interactive();
        } 
}

// ---------------------------------------------------------------------------
// Zend Extension Support
// ---------------------------------------------------------------------------

int apd_zend_startup(zend_extension *extension)
{
	TSRMLS_FETCH();
	CG(extended_info) = 1;  // XXX: this is ridiculous
	return zend_startup_module(&apd_module_entry);
}

ZEND_DLEXPORT void apd_zend_shutdown(zend_extension *extension)
{
	// Do nothing.
}

#ifndef ZEND_EXT_API
#define ZEND_EXT_API	ZEND_DLEXPORT
#endif
ZEND_EXTENSION();

ZEND_DLEXPORT zend_extension zend_extension_entry = {
	"Advanced PHP Debugger (APD)",
	"0.1",
	"Daniel Cowgill and George Schlossnagle",
	"http://apd.communityconnect.com/",
	"Copyright (c) 2001 Community Connect Inc.",
	apd_zend_startup,
	apd_zend_shutdown,
	NULL,		// activate_func_t
	NULL,		// deactivate_func_t
	NULL,		// message_handler_func_t
	NULL,		// op_array_handler_func_t
       onStatement,    // statement_handler_func_t
	fcallBegin,	// fcall_begin_handler_func_t
	fcallEnd,	// fcall_end_handler_func_t
	NULL,		// op_array_ctor_func_t
	NULL,		// op_array_dtor_func_t
#ifdef COMPAT_ZEND_EXTENSION_PROPERTIES
	NULL,		// api_no_check
	COMPAT_ZEND_EXTENSION_PROPERTIES
#else
	STANDARD_ZEND_EXTENSION_PROPERTIES
#endif
};

