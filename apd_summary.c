/* 
   +----------------------------------------------------------------------+
   | APD Profiler & Debugger                                              |
   +----------------------------------------------------------------------+
   | Copyright (c) 2001-2003 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Sterling Hughes <sterling@php.net>                          |
   +----------------------------------------------------------------------+
*/

#include "php_apd.h"

void 
apd_summary_output_header()
{
	apd_file_entry_t *file_entry;
	apd_function_entry_t *function_entry;
	apd_fcall_t *fcall;
	TSRMLS_FETCH();

	APD_GLOBALS(summary).caller = zend_get_executed_filename(TSRMLS_C);
	apd_array_init(&APD_GLOBALS(summary).files, 5, 1.5);
	apd_array_init(&APD_GLOBALS(summary).functions, 20, 1.5);
	zend_llist_init(&APD_GLOBALS(summary).call_list, sizeof(void *), NULL, 0);
}

void
apd_summary_output_file_reference(int id, const char *filename)
{
	apd_file_entry_t *file_entry;
	TSRMLS_FETCH();

	file_entry = emalloc(sizeof(apd_file_entry_t));
	file_entry->id = id;
	file_entry->filename = estrdup(filename);

	apd_array_set(&APD_GLOBALS(summary).files, id, file_entry);
}

void 
apd_summary_output_elapsed_time(int filenum, int linenum, int usert, int systemt, int realt)
{
	zend_llist *list;
	zend_llist_position l;
	apd_fcall_t *cc;
	TSRMLS_FETCH();

	list = &APD_GLOBALS(summary).call_list;

	cc = zend_llist_get_first_ex(list, &l);
	while (cc) {
		cc->usertime += usert;
		cc->systemtime += systemt;
		cc->realtime += realt;

		cc = zend_llist_get_next_ex(list, &l);
	}
}

void
apd_summary_output_declare_function(int index, const char *fname, int type)
{
	apd_function_entry_t *function_entry;
	TSRMLS_FETCH();

	function_entry = ecalloc(1, sizeof(apd_function_entry_t));
	function_entry->name = estrdup(fname);
	function_entry->id = index;
	function_entry->type = type;

	apd_array_set(&APD_GLOBALS(summary).functions, index, function_entry);
}

inline apd_fcall_t *
find_fcall(apd_coverage_t *coverage, int filenum, int linenum)
{
	apd_fcall_t *fcall;
	int i;

	for (fcall = coverage->head; fcall != NULL; fcall = fcall->next) {
		if (fcall->file == filenum && fcall->line == linenum) {
			return fcall;
		}
	}
	
	return NULL;
}

inline void
append_fcall(apd_coverage_t *coverage, apd_fcall_t *fcall) 
{
	coverage->size++;

	if (coverage->head == NULL) {
		coverage->head = coverage->tail = fcall;
		return;
	}

	coverage->tail->next = fcall;
	fcall->prev = coverage->tail;
	coverage->tail = fcall;
}


void
apd_summary_output_enter_function(int index, int filenum, int linenum)
{
	apd_function_entry_t *function_entry;
	apd_coverage_t       *coverage;
	apd_fcall_t          *fcall;
	TSRMLS_FETCH();

	if (index == 1) {
		return;
	}

	function_entry = apd_array_get(&APD_GLOBALS(summary).functions, index);
	if (function_entry == NULL) {
		php_error(E_WARNING, "Couldn't find function entry by index %d", index);
		return;
	}

	fcall = find_fcall(&function_entry->coverage, filenum, linenum);
	if (!fcall) {
		fcall = ecalloc(1, sizeof(apd_fcall_t));
		fcall->line = linenum;
		fcall->file = filenum;
		fcall->entry = function_entry;

		append_fcall(&function_entry->coverage, fcall);
	}
	fcall->calls++;

	zend_llist_add_element(&APD_GLOBALS(summary).call_list, &fcall);
}

static int
compare_fcall(void *e1, void *e2)
{
	apd_fcall_t *a = e1;
	apd_fcall_t *b = e2;

	if (a->entry == b->entry && a->file == b->file && a->line == b->line) {
		return 1;
	}

	return 0;
}

void
apd_summary_output_exit_function(int index, int mem)
{
	apd_fcall_t *fcall;
	void *entry;
	TSRMLS_FETCH();

	entry = apd_array_get(&APD_GLOBALS(summary).functions, index);
	if (entry == NULL) {
		return;
	}

	zend_llist_del_element(&APD_GLOBALS(summary).call_list, entry, compare_fcall);
}

inline void
copy_fcall(apd_fcall_t **a, apd_fcall_t *b)
{
	*a = emalloc(sizeof(apd_fcall_t));
	memcpy(*a, b, sizeof(apd_fcall_t));
	(*a)->prev = NULL;
	(*a)->next = NULL;
}

inline void
place_best_slot(apd_coverage_t *summary, apd_fcall_t *fc, long max)
{
	apd_fcall_t *current = NULL;
	apd_fcall_t *nfc;
	int          is_bigger = 0;

	current = summary->head;
	while (current) {
		if (fc->cumulative >= current->cumulative) {
			is_bigger = 1;
			break;
		}

		current = current->next;
	}

	if (current == NULL) {
		current = summary->tail;
	}

	copy_fcall(&nfc, fc);

	if (!current) {
		summary->head = nfc;
		summary->tail = nfc;
	} else if (current == summary->tail && !is_bigger) {
		nfc->prev = current;
		nfc->next = NULL;
		current->next = nfc;

		summary->tail = nfc;
	} else {
		nfc->next = current;
		nfc->prev = current->prev;
		if (current->prev) {
			current->prev->next = nfc;
		} else {
			summary->head = nfc;
		}
		current->prev = nfc;
	}

	if (++summary->size > max) {
		current = summary->tail;
		if (current->prev) {
			current->prev->next = NULL;
		}
		summary->tail = current->prev;
		efree(current);
	}
}

inline void
add_fcall_summary(apd_coverage_t *summary, apd_fcall_t *fc, long max)
{
	fc->cumulative = fc->realtime + fc->usertime + fc->systemtime;
	place_best_slot(summary, fc, max);
}

inline void
find_expensive(apd_coverage_t *summary, long max TSRMLS_DC)
{
	apd_array_t          *functions;
	apd_function_entry_t *function_entry;
	apd_fcall_t          *fcall;
	long                  i;

	functions = &APD_GLOBALS(summary).functions;
	for (i = 0; i < functions->alloced; ++i) {
		function_entry = (apd_function_entry_t *) apd_array_get(functions, i);
		if (function_entry == NULL) {
			continue;
		}

		for (fcall = function_entry->coverage.head; fcall != NULL; fcall = fcall->next) {
			add_fcall_summary(summary, fcall, max);
		}
	}
}


void
apd_summary_output_footer(void)
{
	apd_fcall_t *fcall;
	apd_function_entry_t *function_entry;
	apd_file_entry_t *file_entry;
	char *shortname;
	apd_coverage_t coverage;
	char *ret;
	int ret_len;
	TSRMLS_FETCH();

	memset(&coverage, 0, sizeof(apd_coverage_t));

	php_printf("<table border=\"1\" width=\"100%\">\n");
	php_printf("<tr>\n");
	php_printf("<th>Function</th>\n");
	php_printf("<th>File</th>\n");
	php_printf("<th>Line</th>\n");
	php_printf("<th># of calls</th>\n");
	php_printf("<th>User</th>\n");
	php_printf("<th>System</th>\n");
	php_printf("<th>Real</th>\n");
	php_printf("</tr>\n");

	find_expensive(&coverage, 20 TSRMLS_CC);
	fcall = coverage.head;
	while (fcall) {
		function_entry = fcall->entry;
		file_entry = apd_array_get(&APD_GLOBALS(summary).files, fcall->file);
		php_basename(file_entry->filename, strlen(file_entry->filename), NULL, 0, &ret, &ret_len TSRMLS_CC);
		php_printf("<tr>\n");
		php_printf("<td>%s</td>\n", function_entry->name);
		php_printf("<td><abbr title=\"%s\">%s</abbr></td>\n", file_entry->filename, ret);
		php_printf("<td>%d</td>\n", fcall->line);
		php_printf("<td>%d</td>\n", fcall->calls);
		php_printf("<td>%4.2f</td>\n", (double) fcall->usertime / 1000000);
		php_printf("<td>%4.2f</td>\n", (double) fcall->systemtime / 1000000);
		php_printf("<td>%4.2f</td>\n", (double) fcall->realtime / 1000000);
		php_printf("</tr>\n");
		
		fcall = fcall->next;
//		efree(ret);
	}
	php_printf("</table>\n");

	zend_llist_clean(&APD_GLOBALS(summary).call_list);
}

/**
 * Local Variables:
 * indent-tabs-mode: t
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600:fdm=marker;
 * vim:noet:sw=4:ts=4;
 */
