/* 
   +----------------------------------------------------------------------+
   | APD Profiler & Debugger											  |
   +----------------------------------------------------------------------+
   | Copyright (c) 2001-2003 Community Connect Inc.					   |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,	  |
   | that is bundled with this package in the file LICENSE, and is		|
   | available at through the world-wide-web at						   |
   | http://www.php.net/license/2_02.txt.								 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to		  |
   | license@php.net so we can mail you a copy immediately.			   |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>			  |
   |		  George Schlossnagle <george@lethargy.org>				   |
   |		  Sterling Hughes <sterling@php.net>						  |
   +----------------------------------------------------------------------+
*/
	
#include "php_apd.h"
#ifdef PHP_WIN32
#include "win32compat.h"
# define APD_IS_INVALID_SOCKET(a)   (a == INVALID_SOCKET)
#else
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>
# define APD_IS_INVALID_SOCKET(a)   (a < 0)
#endif

#include "apd_lib.h"
#include "zend_API.h"
#include "zend_hash.h"
#include "zend_alloc.h"
#include "zend_operators.h"
#include "zend_globals.h"
#include "zend_compile.h"
#include <assert.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef SUN_LEN
#define SUN_LEN(su) (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

#ifndef PF_INET
#define PF_INET AF_INET
#endif

/**
 * Tracng calls to zend_compile_file
 */
#undef TRACE_ZEND_COMPILE /* define to trace all calls to zend_compile_file */
ZEND_DLEXPORT zend_op_array* apd_compile_file(zend_file_handle* TSRMLS_DC);
ZEND_DLEXPORT zend_op_array* (*old_compile_file)(zend_file_handle* TSRMLS_DC);
ZEND_DLEXPORT void apd_execute(zend_op_array *op_array TSRMLS_DC);
#if  ZEND_EXTENSION_API_NO >= 20020731
ZEND_DLEXPORT void apd_execute_internal(zend_execute_data *execute_data_ptr, int return_value_used TSRMLS_DC);
#endif
ZEND_DLEXPORT void (*old_execute)(zend_op_array *op_array TSRMLS_DC);

ZEND_DLEXPORT void onStatement(zend_op_array *op_array TSRMLS_DC);
ZEND_DECLARE_MODULE_GLOBALS(apd);

/* This comes from php install tree. */
#include "ext/standard/info.h"

/* List of exported functions. */

function_entry apd_functions[] = {
	PHP_FE(override_function, NULL)
	PHP_FE(rename_function, NULL)
	PHP_FE(apd_set_pprof_trace, NULL)
	PHP_FE(apd_set_browser_trace, NULL)
	PHP_FE(apd_set_session_trace_socket, NULL)
	PHP_FE(apd_breakpoint, NULL)
	PHP_FE(apd_continue, NULL)
	PHP_FE(apd_echo, NULL)
	{NULL, NULL, NULL}
};

void apd_pprof_fprintf(const char* fmt, ...);
static void log_time(TSRMLS_D);

long diff_times(struct timeval a, struct timeval b)
{
	long retval = (a.tv_sec - b.tv_sec) * 1000000L;
	retval += (a.tv_usec - b.tv_usec);

	return retval;
}

/**
 * Output to be analyzed by pprof
 */
static void 
apd_pprof_output_header(void)
{
	TSRMLS_FETCH();

	apd_pprof_fprintf("#Pprof [APD] v1.0.1\n");
	apd_pprof_fprintf("caller=%s\n",zend_get_executed_filename(TSRMLS_C));
	apd_pprof_fprintf("\nEND_HEADER\n");
}

static void
apd_pprof_output_file_reference(int id, const char *filename)
{
	apd_pprof_fprintf("! %d %s\n", id, filename);
}

static void 
apd_pprof_output_elapsed_time(int filenum, int linenum, int usert, int systemt, int realt)
{
	apd_pprof_fprintf("@ %d %d %ld %ld %ld\n", filenum, linenum, usert, systemt, realt);
}

static void
apd_pprof_output_declare_function(int index, const char *fname, int type)
{
	apd_pprof_fprintf("& %d %s %d\n", index, fname, type);
}

static void
apd_pprof_output_enter_function(int index, int filenum, int linenum)
{
	apd_pprof_fprintf("+ %d %d %d\n", index, filenum, linenum);
}

static void
apd_pprof_output_exit_function(int index, int mem)
{
	if (mem == -1) {
		apd_pprof_fprintf("- %d\n", index);
	} else {
		apd_pprof_fprintf("- %d %d\n", index, mem);
	}
}

static void
apd_pprof_output_footer(void)
{
	struct rusage end_ru;
	struct timeval end_clock;
	TSRMLS_FETCH();
	
	if (!APD_GLOBALS(pprof_file)) {
		return;
	}

	gettimeofday(&end_clock, NULL);
	getrusage(RUSAGE_SELF, &end_ru);

	apd_pprof_fprintf("END_TRACE\n");
	apd_pprof_fprintf("total_user=%ld\ntotal_sys=%ld\ntotal_wall=%ld\n",
					  diff_times(end_ru.ru_utime, APD_GLOBALS(first_ru).ru_utime),
					  diff_times(end_ru.ru_stime, APD_GLOBALS(first_ru).ru_stime),
					  diff_times(end_clock, APD_GLOBALS(first_clock)));
	apd_pprof_fprintf("END_FOOTER\n");
}


/* ---------------------------------------------------------------------------
   apd_dump_fprintf - Outputer - either to file or socket
   --------------------------------------------------------------------------- */
void apd_dump_fprintf(const char* fmt, ...)
{
	va_list args;
	char* newStr;
	TSRMLS_FETCH();	
	va_start(args, fmt);
	newStr = apd_sprintf_real(fmt, args);
	va_end(args);
	
	if (APD_GLOBALS(dump_file) != NULL) {
		fprintf(APD_GLOBALS(dump_file), "%s", newStr);
	} else if ( APD_GLOBALS(dump_sock_id) > 0)  {
#ifndef PHP_WIN32
		write(APD_GLOBALS(dump_sock_id), newStr, strlen (newStr) + 1);
#else
		send(APD_GLOBALS(dump_sock_id), newStr, strlen (newStr) + 1, 0);
#endif
	}
	efree(newStr);
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
	efree(newStr);
}


/* ---------------------------------------------------------------------------------
   Interactive Mode
   --------------------------------------------------------------------------------- */
 
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
	
	tmpbuf = emalloc(length + 1);
	
	recv_len = recv(APD_GLOBALS(dump_sock_id),   tmpbuf , length, 0) ;
	if (recv_len == -1) {
		php_error(E_WARNING, "apd debugger failed to recieve data: turning off debugger");
		efree(tmpbuf);
		APD_GLOBALS(interactive_mode) = 0;
		return;
	}
	tmpbuf = erealloc(tmpbuf, recv_len + 1);
	tmpbuf[ recv_len ] = '\0' ;
	
	if (strcmp(tmpbuf,"\n")==0) {
		efree(tmpbuf);
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
	efree(tmpbuf);
	
	/* call myself again! = recursive! */
	apd_interactive();
}

 

/* ---------------------------------------------------------------------------
   Call Tracing (Application-specific Code)
   --------------------------------------------------------------------------- */

char *apd_get_active_function_name(zend_op_array *op_array TSRMLS_DC)
{
	char *funcname = NULL;
	int curSize = 0;
	zend_execute_data *execd = NULL;
	char *tmpfname;
	char *classname;
	int classnameLen;
	int tmpfnameLen;
  
	execd = EG(current_execute_data);
	if(execd) {
		tmpfname = execd->function_state.function->common.function_name;
		if(tmpfname) {
			tmpfnameLen = strlen(tmpfname);
			if(execd->object) {
				classname = Z_OBJCE(*execd->object)->name;
				classnameLen = strlen(classname);
				funcname = (char *)emalloc(classnameLen + tmpfnameLen + 3);
				snprintf(funcname, classnameLen + tmpfnameLen + 3, "%s->%s",
						 classname, tmpfname);
			}
			else if(execd->function_state.function->common.scope) {
				classname = execd->function_state.function->common.scope->name;
				classnameLen = strlen(classname);
				funcname = (char *)emalloc(classnameLen + tmpfnameLen + 3);
				snprintf(funcname, classnameLen + tmpfnameLen + 3, "%s::%s",
						 classname, tmpfname);
			}
			else {
				funcname = estrdup(tmpfname);
			}
		} 
		else {
			switch (execd->opline->op2.u.constant.value.lval) {
			case ZEND_EVAL:
				funcname = estrdup("eval");
				break;
			case ZEND_INCLUDE:
				funcname = estrdup("include");
				break;
			case ZEND_REQUIRE:
				funcname = estrdup("require");
				break;
			case ZEND_INCLUDE_ONCE:
				funcname = estrdup("include_once");
				break;
			case ZEND_REQUIRE_ONCE:
				funcname = estrdup("require_once");
				break;
			default:
				funcname = estrdup("???");
				break;
			}
		}
	} 
	else {
		funcname = estrdup("main");
	}
	return funcname;
}

static void trace_function_entry(HashTable *func_table, const char *fname, int type, const char *filename, int linenum)
{
	int i = 0;
	int *function_index;
	int *filenum;
	TSRMLS_FETCH();

	if (!APD_GLOBALS(pproftrace)) {
		return;
	}
	
	if (zend_hash_find(APD_GLOBALS(file_summary), (char *) filename, strlen(filename) + 1, (void *) &filenum) == FAILURE) {
		filenum = (int *) emalloc(sizeof(int));
		*filenum = ++APD_GLOBALS(file_index);
		APD_GLOBALS(output).file_reference(*filenum, filename);
		zend_hash_add(APD_GLOBALS(file_summary), (char *) filename, strlen(filename) + 1, filenum, sizeof(int), NULL);
	}
	APD_GLOBALS(current_file_index) = *filenum;

	if (zend_hash_find(APD_GLOBALS(function_summary), (char *) fname, strlen(fname)+1, (void *) &function_index) == SUCCESS) {
		APD_GLOBALS(output).enter_function(*function_index, *filenum, linenum);
	} else {
		function_index = (int *) emalloc(sizeof(int));
		*function_index = ++APD_GLOBALS(function_index);
		zend_hash_add(APD_GLOBALS(function_summary), (char *) fname, strlen(fname)+1, function_index, sizeof(int), NULL);
		APD_GLOBALS(output).declare_function(*function_index, fname, type);
		APD_GLOBALS(output).enter_function(*function_index, *filenum, linenum);
	}
}


static void trace_function_exit(char *fname)
{
	struct rusage wall_ru;
	struct timeval clock;
	long utime, stime, rtime;
	int  *function_index;
	int   allocated = -1;
	TSRMLS_FETCH();

	if (!APD_GLOBALS(pproftrace)) {
		return;
	}

#if MEMORY_LIMIT
	allocated = AG(memory_limit);
#endif
	log_time(TSRMLS_C);	
	if (zend_hash_find(APD_GLOBALS(function_summary), fname, strlen(fname) + 1, (void *) &function_index) == SUCCESS) {
		APD_GLOBALS(output).exit_function(*function_index, allocated);
	} else {
		function_index = (int *) emalloc(sizeof(int));
		*function_index = ++APD_GLOBALS(function_index);
		zend_hash_add(APD_GLOBALS(function_summary), fname, strlen(fname)+1, function_index, sizeof(int), NULL);
		APD_GLOBALS(output).exit_function(*function_index, allocated);
	}
}

static void log_time(TSRMLS_D)
{
	struct timeval clock;
	struct rusage wall_ru;

	gettimeofday(&clock, NULL);
	getrusage(RUSAGE_SELF, &wall_ru);

	if (APD_GLOBALS(function_index) > 1) {
		long utime, stime, rtime;
		utime = diff_times(wall_ru.ru_utime, APD_GLOBALS(last_ru).ru_utime);
		stime = diff_times(wall_ru.ru_stime, APD_GLOBALS(last_ru).ru_stime);
		rtime = diff_times(clock, APD_GLOBALS(last_clock));
		if(utime || stime || rtime) {
			APD_GLOBALS(output).elapsed_time(APD_GLOBALS(current_file_index), zend_get_executed_lineno(TSRMLS_C), utime, stime, rtime);
		}
	}
	
	APD_GLOBALS(last_ru) = wall_ru;
	APD_GLOBALS(last_clock) = clock;
}

/* --------------------------------------------------------------------------
   Module Entry
   --------------------------------------------------------------------------- */
zend_module_entry apd_module_entry = {
	STANDARD_MODULE_HEADER,
	"APD",
	apd_functions,
	PHP_MINIT(apd),
	NULL,
	PHP_RINIT(apd),
	PHP_RSHUTDOWN(apd),
	PHP_MINFO(apd),
	NO_VERSION_YET,	/* extension version number (string) */
	STANDARD_MODULE_PROPERTIES
};

#if COMPILE_DL_APD
ZEND_GET_MODULE(apd)
#endif

/* ---------------------------------------------------------------------------
   PHP Configuration Functions
   --------------------------------------------------------------------------- */

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
	 PHP_INI_ENTRY("apd.dumpdir", NULL,   PHP_INI_ALL, set_dumpdir)
	 STD_PHP_INI_ENTRY("apd.statement_tracing", "0",   PHP_INI_ALL, OnUpdateLong, statement_tracing, zend_apd_globals, apd_globals)
PHP_INI_END()


/* ---------------------------------------------------------------------------
   Module Startup and Shutdown Function Definitions
   --------------------------------------------------------------------------- */

static void php_apd_init_globals(zend_apd_globals *apd_globals) 
{
	memset(apd_globals, 0, sizeof(zend_apd_globals));   
	
	apd_globals->function_summary = (HashTable *) malloc(sizeof(HashTable));
	apd_globals->file_summary = (HashTable *) malloc(sizeof(HashTable));

	zend_hash_init(apd_globals->function_summary, 0, NULL, NULL, 1);
	zend_hash_init(apd_globals->file_summary, 0, NULL, NULL, 1);

}

static void php_apd_free_globals(zend_apd_globals *apd_globals)
{
	free(apd_globals->function_summary);
	free(apd_globals->file_summary);
}

PHP_MINIT_FUNCTION(apd)
{
	ZEND_INIT_MODULE_GLOBALS(apd, php_apd_init_globals, php_apd_free_globals);
	REGISTER_INI_ENTRIES();
	old_execute = zend_execute;
	zend_execute = apd_execute;
	zend_execute_internal = apd_execute_internal;
	return SUCCESS;
}


ZEND_API void apd_execute(zend_op_array *op_array TSRMLS_DC) 
{
	char *fname = NULL;
	char *tmp;
	void **p;
	int argCount;
	zval **object_ptr_ptr;
  	fname = apd_get_active_function_name(op_array TSRMLS_CC);
   	trace_function_entry(EG(function_table), fname, ZEND_USER_FUNCTION,
						zend_get_executed_filename(TSRMLS_C),
						zend_get_executed_lineno(TSRMLS_C));
   	old_execute(op_array TSRMLS_CC);
   	trace_function_exit(fname);
   	efree(fname);
	apd_interactive();
}

ZEND_API void apd_execute_internal(zend_execute_data *execute_data_ptr, int return_value_used TSRMLS_DC) 
{
	char *fname = NULL;
	char *tmp;
	void **p;
	int argCount;
	zval **object_ptr_ptr;
	zend_execute_data *execd;

	execd = EG(current_execute_data);
   	fname = apd_get_active_function_name(execd->op_array TSRMLS_CC);
   	trace_function_entry(EG(function_table), fname, ZEND_INTERNAL_FUNCTION,
						zend_get_executed_filename(TSRMLS_C),
						zend_get_executed_lineno(TSRMLS_C));
	execute_internal(execute_data_ptr, return_value_used TSRMLS_CC);
	trace_function_exit(fname);
	efree(fname);
	apd_interactive();
}
	


PHP_RINIT_FUNCTION(apd)
{
	APD_GLOBALS(output).header = apd_pprof_output_header;
	APD_GLOBALS(output).footer = apd_pprof_output_footer;
	APD_GLOBALS(output).file_reference = apd_pprof_output_file_reference;
	APD_GLOBALS(output).elapsed_time = apd_pprof_output_elapsed_time;
	APD_GLOBALS(output).declare_function = apd_pprof_output_declare_function;
	APD_GLOBALS(output).enter_function = apd_pprof_output_enter_function;
	APD_GLOBALS(output).exit_function = apd_pprof_output_exit_function;

	APD_GLOBALS(dump_file) = stderr;
	APD_GLOBALS(dump_sock_id) = 0;
	APD_GLOBALS(interactive_mode) = 0;
	APD_GLOBALS(ignore_interactive) = 0;  
	gettimeofday(&APD_GLOBALS(last_clock), NULL);
	getrusage(RUSAGE_SELF, &APD_GLOBALS(last_ru));
	APD_GLOBALS(first_ru) = APD_GLOBALS(last_ru);
	APD_GLOBALS(first_clock) = APD_GLOBALS(last_clock);
	
	/**
	 * These values must start from 1 because they are stored in the hashtable, and
	 * Zend will try to free them, and die freeing a NULL pointer otherwise.
	 */
	APD_GLOBALS(file_index) = 1;
	APD_GLOBALS(function_index) = 1;

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apd)
{
//	APD_GLOBALS(output).footer();
	if (APD_GLOBALS(pprof_file)) {
		fclose(APD_GLOBALS(pprof_file));
	}

	if (APD_GLOBALS(dump_sock_id)) {
		close(APD_GLOBALS(dump_sock_id));
		/* bit academic - but may as well */
		APD_GLOBALS(dump_sock_id)=0;
	}

	zend_hash_clean(APD_GLOBALS(function_summary));
	zend_hash_clean(APD_GLOBALS(file_summary));
	APD_GLOBALS(counter)++;
	return SUCCESS;
}

PHP_MINFO_FUNCTION(apd)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Advanced PHP Debugger (APD)", "Enabled");
	php_info_print_table_row(2, "APD Version", APD_VERSION);
	php_info_print_table_end();
}


/* ---------------------------------------------------------------------------
   PHP Extension Functions
   --------------------------------------------------------------------------- */

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

void apd_pprof_header(char *ent_fname TSRMLS_DC) {
	char *fname = "main";
	char *filename;
	int linenum, *filenum, *fnum;

	APD_GLOBALS(output).header();

	filename = zend_get_executed_filename(TSRMLS_C);
	linenum = zend_get_executed_lineno(TSRMLS_C);

	fnum = (int *) emalloc(sizeof(int));
	*fnum = APD_GLOBALS(function_index)++;
	zend_hash_add(APD_GLOBALS(function_summary), fname, strlen(fname)+1, fnum, sizeof(int), NULL);

	filenum = (int *) emalloc(sizeof(int));
	*filenum = APD_GLOBALS(file_index)++;
	zend_hash_add(APD_GLOBALS(file_summary), (char *) filename, strlen(filename)+1, filenum, sizeof(int), NULL);

	APD_GLOBALS(output).file_reference(*filenum, filename);
	APD_GLOBALS(output).declare_function(*fnum, fname, ZEND_USER_FUNCTION);
	APD_GLOBALS(output).enter_function(*fnum, *filenum,  linenum);

	fnum = (int *) emalloc(sizeof(int));
	*fnum = APD_GLOBALS(function_index)++;
	zend_hash_add(APD_GLOBALS(function_summary), ent_fname, strlen(ent_fname)+1, fnum, sizeof(int), NULL);

	APD_GLOBALS(output).declare_function(*fnum, ent_fname, ZEND_USER_FUNCTION);
	APD_GLOBALS(output).enter_function(*fnum, *filenum,  linenum);
}

PHP_FUNCTION(apd_set_browser_trace)
{
	if (ZEND_NUM_ARGS() > 1) {
		ZEND_WRONG_PARAM_COUNT();
	}
	
	APD_GLOBALS(pproftrace) = 1;

	APD_GLOBALS(output).header = apd_summary_output_header;
	APD_GLOBALS(output).footer = apd_summary_output_footer;
	APD_GLOBALS(output).file_reference = apd_summary_output_file_reference;
	APD_GLOBALS(output).elapsed_time = apd_summary_output_elapsed_time;
	APD_GLOBALS(output).declare_function = apd_summary_output_declare_function;
	APD_GLOBALS(output).enter_function = apd_summary_output_enter_function;
	APD_GLOBALS(output).exit_function = apd_summary_output_exit_function;

	apd_pprof_header("apd_set_broswer_trace" TSRMLS_CC);
}

PHP_FUNCTION(apd_set_pprof_trace)
{
	int issock =0;
	int socketd = 0;
	int path_len;
	char *dumpdir;
	char path[MAXPATHLEN];
	zval  **z_dumpdir;

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
	
	snprintf(path, MAXPATHLEN, "%s/pprof.%05d.%d", dumpdir, getpid(), APD_GLOBALS(counter));
	if((APD_GLOBALS(pprof_file) = fopen(path, "a")) == NULL) {
		zend_error(E_ERROR, "%s() failed to open %s for tracing", get_active_function_name(TSRMLS_C), path);
	}  

	apd_pprof_header("apd_set_pprof_trace" TSRMLS_CC);
}  


/* {{{ proto bool apd_set_session_trace_socket(string ip_or_filename, int domain, int port, int mask)
   Connects to a socket either unix domain or tcpip and sends data there*/

PHP_FUNCTION(apd_set_session_trace_socket) 
{
 
	char *address;
	int address_len;
 
	int sock_domain;
	int sock_port;
	int connect_result;
   
	struct sockaddr_in si;
	struct sockaddr_un su;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, 
							  "slll", &address,&address_len, &sock_domain, &sock_port) == FAILURE) 
		{
			return;
		}
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
	apd_summary_output_header();
	RETURN_TRUE;
}

/* }}} */
/* {{{ proto void apd_breakpoint()
   Starts interactive mode*/

PHP_FUNCTION(apd_breakpoint) 
{
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, 
							  "l") == FAILURE) {
		return;
	} 
	APD_GLOBALS(interactive_mode) = 1;
	RETURN_TRUE;
}

/* }}} */
/* {{{ proto void apd_continue()
   Stops interactive mode - normally called by interactive interpreter! */

PHP_FUNCTION(apd_continue) 
{
	APD_GLOBALS(interactive_mode) = 0;
	RETURN_TRUE;
}

/* }}} */
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
		/*	 php_error(E_WARNING, "apd echoing %s", str); */
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
		/*	 efree(str); */
	}
	RETURN_TRUE;

}

/* }}} */
 

// ---------------------------------------------------------------------------
// Zend Extension Functions
// ---------------------------------------------------------------------------

ZEND_DLEXPORT void onStatement(zend_op_array *op_array)
{
	TSRMLS_FETCH();
	if(APD_GLOBALS(pproftrace) && APD_GLOBALS(statement_tracing)) {
		log_time(TSRMLS_C);
	}
}

int apd_zend_startup(zend_extension *extension)
{
	TSRMLS_FETCH();
	CG(extended_info) = 1;  /* XXX: this is ridiculous */
	return zend_startup_module(&apd_module_entry);
}

ZEND_DLEXPORT void apd_zend_shutdown(zend_extension *extension)
{
	/* Do nothing. */
}

#ifndef ZEND_EXT_API
#define ZEND_EXT_API	ZEND_DLEXPORT
#endif
ZEND_EXTENSION();

ZEND_DLEXPORT zend_extension zend_extension_entry = {
	"Advanced PHP Debugger (APD)",
	APD_VERSION,
	"George Schlossnagle",
	"http://pear.php.net/",
	"",
	apd_zend_startup,
	apd_zend_shutdown,
	NULL,	   // activate_func_t
	NULL,	   // deactivate_func_t
	NULL,	   // message_handler_func_t
	NULL,	   // op_array_handler_func_t
	onStatement, // statement_handler_func_t
	NULL,   // fcall_begin_handler_func_t
	NULL,   // fcall_end_handler_func_t
	NULL,	   // op_array_ctor_func_t
	NULL,	   // op_array_dtor_func_t
	NULL,	   // api_no_check
	COMPAT_ZEND_EXTENSION_PROPERTIES
};

/**
 * Local Variables:
 * indent-tabs-mode: t
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600:fdm=marker
 * vim:noet:sw=4:ts=4
 */
