#ifndef EL_DOM_SGML_DUMP_H
#define EL_DOM_SGML_DUMP_H

#include <stdio.h>

struct dom_stack;
struct dom_stack_context;

struct dom_stack_context *
add_sgml_file_dumper(struct dom_stack *stack, FILE *file);

#endif
