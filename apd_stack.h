#ifndef INCLUDED_APD_STACK
#define INCLUDED_APD_STACK

#define T apd_stack_t*
typedef struct apd_stack_t apd_stack_t;

extern T		apd_stack_create		();
extern void		apd_stack_destroy		(T stack);
extern void		apd_stack_push			(T stack, void* ptr);
extern void*	apd_stack_pop			(T stack);
extern int		apd_stack_isempty		(T stack);
extern void**	apd_stack_toarray		(T stack);
extern int		apd_stack_getsize		(T stack);
extern void		apd_stack_apply			(T stack, void (*funcPtr)(void*));

#undef T
#endif
