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

#include "apd_stack.h"
#include "apd_lib.h"
#include <assert.h>
#include <stdlib.h>

enum { START_SIZE = 1, GROW_FACTOR = 2 };

struct apd_stack_t {
	int top;
	int size;
	void** elements;
};

apd_stack_t* apd_stack_create()
{
	apd_stack_t* s;

	s = (apd_stack_t*) apd_emalloc(sizeof(apd_stack_t));
	s->top = 0;
	s->size = START_SIZE;
	s->elements = apd_emalloc(sizeof(void*) * s->size);
	return s;
}

void apd_stack_destroy(apd_stack_t* stack)
{
	apd_efree(stack->elements);
	apd_efree(stack);
}

void apd_stack_push(apd_stack_t* stack, void* ptr)
{
	if (stack->top >= stack->size) {
		while (stack->top >= stack->size) {
			stack->size *= GROW_FACTOR;
		}
		stack->elements = apd_erealloc(stack->elements,
			stack->size * sizeof(void*));
	}
	stack->elements[stack->top++] = ptr;
}

void* apd_stack_pop(apd_stack_t* stack)
{
	assert(!apd_stack_isempty(stack));
	return stack->elements[--stack->top];
}

int apd_stack_isempty(apd_stack_t* stack)
{
	return stack->top == 0;
}

void** apd_stack_toarray(apd_stack_t* stack)
{
	return stack->elements;
}

int apd_stack_getsize(apd_stack_t* stack)
{
	return stack->top;
}

void apd_stack_apply(apd_stack_t* stack, void (*funcPtr)(void*))
{
	int i;

	for (i = 0; i < stack->top; i++) {
		funcPtr(stack->elements[i]);
	}
}
