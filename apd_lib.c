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

#include "apd_lib.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "zend.h"
#include "php.h"
#include "zend.h"

#ifndef PHP_WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

#undef DEBUG

/* apd_emalloc: malloc that dies on failure */
void* apd_emalloc(size_t n)
{
	void* p = malloc(n);
	if (p == NULL) {
		apd_eprint("apd_emalloc: malloc failed to allocate %u bytes:", n);
	}
	return p;
}

/* apd_erealloc: realloc that dies on failure */
void* apd_erealloc(void* p, size_t n)
{
	p = realloc(p, n);
	if (p == NULL) {
		apd_eprint("apd_erealloc: realloc failed to allocate %u bytes:", n);
	}
	return p;
}

/* apd_efree: free that bombs when given a null pointer */
void apd_efree(void* p)
{
	if (p == NULL) {
		apd_eprint("apd_efree: attempt to free null pointer");
	}
	free(p);
}

/* apd_estrdup: strdup that dies on failure */
char* apd_estrdup(const char* s)
{
	int len;
	char* dup;

	if (s == NULL) {
		return NULL;
	}
	len = strlen(s);
	dup = (char*) malloc(len+1);
	if (dup == NULL) {
		apd_eprint("apd_estrdup: malloc failed to allocate %u bytes:", len+1);
	}
	memcpy(dup, s, len);
	dup[len] = '\0';
	return dup;
}

/* apd_copystr: copy string of known length */
char* apd_copystr(const char* s, int len)
{
	char* dup;

	if (s == NULL) {
		return NULL;
	}
	dup = (char*) malloc(len+1);
	if (dup == NULL) {
		apd_eprint("apd_estrdup: malloc failed to allocate %u bytes:", len+1);
	}
	memcpy(dup, s, len);
	dup[len] = '\0';
	return dup;
}

/* apd_eprint: print error message and exit */
void apd_eprint(char *fmt, ...)
{
	va_list args;

	fflush(stdout);
	
	va_start(args, fmt);
	va_end(args);

	if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':') {
	}
	exit(2);
}

/* apd_dprint: print messages if DEBUG is defined */
void apd_dprint(char *fmt, ...)
{
#ifdef DEBUG
	va_list args;

	printf("DEBUG: ");

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':') {
		printf(" %s", strerror(errno));
	}
#endif
}

/* apd_strcat: like strcat, but automatically expands dest string */
void apd_strcat(char** dst, int* curSize, const char* src)
{
	int dstLen;
	int srcLen;

	srcLen = strlen(src);

	if (*dst == 0) {
		*curSize = srcLen+1;
		*dst = (char*) malloc(*curSize);
		strcpy(*dst, src);
		return;
	}

	dstLen = strlen(*dst);
	if (dstLen + srcLen + 1 > *curSize) {
		while (dstLen + srcLen + 1 > *curSize) {
			if(*curSize == 0) {
				*curSize = 1;
			}
			else {
				*curSize *= 2;
			}
		}
		*dst = realloc(*dst, *curSize);
	}

	strcat(*dst, src);
}

/* apd_strncat: like strcat, but automatically expands dest string */
void apd_strncat(char** dst, int* curSize, const char* src, int srcLen)
{
    int dstLen;

    if (*dst == 0) {
        *curSize = srcLen+1;
        *dst = (char*) malloc(*curSize);
        strncpy(*dst, src, srcLen);
        return;
    }

    dstLen = strlen(*dst);

    if (dstLen + srcLen + 1 > *curSize) {
		while (dstLen + srcLen + 1 > *curSize) {
	        *curSize *= 2;
		}
        *dst = realloc(*dst, *curSize);
    }

    strncat(*dst, src, dstLen + srcLen);
}

/* apd_sprintf: safe, automatic sprintf */
char* apd_sprintf(const char* fmt, ...)
{
	char* newStr;
	int size;
	va_list args;

	size = 1;
	newStr = (char*) apd_emalloc(size);

	va_start(args, fmt);
	for (;;) {
		int n = vsnprintf(newStr, size, fmt, args);
		if (n > -1 && n < size) {
			break;
		}
		if(n < 0 ) {
			size *= 2;
		}
		else {
			size = n+1;
		}
		newStr = (char*) apd_erealloc(newStr, size);
	}
	va_end(args);

	return newStr;
}

char* apd_sprintcatf(char** dst, const char* fmt, ...)
{
	char* tmpStr;
	int dstLen;
	int curLen, size , curSize;
	va_list args;

	size = 1;
	tmpStr = (char*) apd_emalloc(size);


	va_start(args, fmt);
	for(;;) {
		int n = vsnprintf(tmpStr, size, fmt, args);
		if( n > -1 && n < size) {
			break;
		}
		if( n < 0 ) {
			if(size == 0) {
				size = 1;
			}
			else {
				size *= 2;
			}
		}
		else {
			size = n+1;
		}
		tmpStr = (char*) apd_erealloc(tmpStr, size);
	}
	va_end(args);
	if(*dst == NULL) {
		*dst = tmpStr;
		return tmpStr;
	}
	curSize = strlen(*dst) + 1;
	apd_strcat(dst, &curSize , tmpStr);
	return *dst;
}

char* apd_strtac(char **dst, char *src) 
{
	int dstLen, srcLen;
	char *tmpStr;
	
	if(*dst == NULL) {
		*dst = (char *) apd_emalloc(strlen(src) + 1);
		strcpy(*dst,src);
		return *dst;
	}
	dstLen = strlen(*dst);
	srcLen = strlen(src);
	tmpStr = (char*) apd_emalloc(dstLen + srcLen +1);
	memcpy(tmpStr, src, srcLen);
	strcat(tmpStr, *dst);
	apd_efree(*dst);
	*dst = tmpStr;
	return *dst;
}
	
char* apd_indent(char **dst, int spaces)
{
	char *tmpStr;
	int dstLen;

	if(spaces == 0) {
		return *dst;
	}
	if(*dst == NULL) {
		dstLen = 0;
	}
	else {
		dstLen = strlen(*dst);
	}
	tmpStr = (char*) apd_emalloc(spaces + dstLen + 1);
	memset(tmpStr, ' ', spaces);
	tmpStr[spaces] = '\0';
	if(dstLen == 0)
	{
		*dst = tmpStr; // FIXME: memory leak
		return *dst;
	}
	strcat(tmpStr, *dst);
	apd_efree(*dst);
	*dst = tmpStr;
	return *dst;
}	
	
int __apd_dump_regular_resources(zval *arr TSRMLS_DC) {
	Bucket *p;
	HashTable *ht;

	if(array_init(arr) == FAILURE) {
		fprintf(stderr, "failed\n");
		return 0;
	}
	ht = &(EG(regular_list));
	p = ht->pListHead;
	while(p != NULL) {
		list_entry *le;
		char *resource_name = NULL;
		le = p->pData;
		resource_name = zend_rsrc_list_get_rsrc_type(p->h TSRMLS_CC);
		if (resource_name != NULL) {
			add_index_string(arr, p->h, zend_rsrc_list_get_rsrc_type(p->h TSRMLS_CC), 1);
		} else {
			resource_name = apd_emalloc(256);
			snprintf(resource_name, 255, "APD: unknown resource type %d", p->h);
			add_index_string(arr, p->h, resource_name, 1);
			apd_efree(resource_name);
		}
		p = p->pListNext;
	}
	return 0;
}

int __apd_dump_persistent_resources(zval *arr TSRMLS_DC) {
	Bucket *p;
	HashTable *ht;
	if(array_init(arr) == FAILURE) {
		return 0;
	}
	ht = &(EG(persistent_list));
	p = ht->pListHead;
	while( p != NULL) {
		list_entry *le;
		le =p->pData;
		add_next_index_stringl(arr, p->arKey, p->nKeyLength, 1);
		p = p->pListNext;
	}
	return 0;
}

void timevaldiff(struct timeval *a, struct timeval *b, struct timeval *result)
{
	result->tv_sec = a->tv_sec - b->tv_sec;          
	result->tv_usec = a->tv_usec - b->tv_usec; 
	if (result->tv_usec < 0) {
		--(result->tv_sec); 
		result->tv_usec += 1000000;
	}
}
		

		
	
