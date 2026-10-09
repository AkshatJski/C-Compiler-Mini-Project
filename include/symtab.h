#ifndef SYMTAB_H
#define SYMTAB_H

#include "ast.h"

typedef struct Symbol {
    char *name;
    TypeKind type;     /* variable/parameter type, or function return type */
    int stack_offset;  /* local and parameter slots: -off(%rbp); globals: 0 */
    int is_global;
    int is_function;
    int param_count;
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    Symbol *symbols;
    struct Scope *parent;
} Scope;

void symtab_init(void);
void scope_push(void);
void scope_pop(void);
Symbol *symbol_insert(const char *name, TypeKind type, int is_global);
Symbol *symbol_lookup(const char *name);
Symbol *symbol_lookup_current(const char *name);

#endif /* SYMTAB_H */
