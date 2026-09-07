#ifndef SYMTAB_H
#define SYMTAB_H

#include "tokens.h"

typedef enum {
    SYMBOL_TYPE_VOID,
    SYMBOL_TYPE_INT
} SymbolType;

typedef struct Symbol {
    char *name;
    SymbolType type;
    int stack_offset;  /* local and parameter slots: -off(%rbp); globals: 0 */
    int is_global;
    int is_function;
    int is_param;
    int param_count;
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    Symbol *symbols;
    struct Scope *parent;
    struct Scope *child;
} Scope;

void symtab_init(void);
void scope_push(void);
void scope_pop(void);
int scope_depth(void);
Symbol *symbol_insert(const char *name, SymbolType type, int is_global);
Symbol *symbol_lookup(const char *name);
Symbol *symbol_lookup_current(const char *name);

#endif /* SYMTAB_H */
