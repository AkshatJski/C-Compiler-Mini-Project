#ifndef DUMP_H
#define DUMP_H

#include <stdio.h>

#include "ast.h"
#include "lexer.h"

/* Diagnostics used by the --dump-tokens and --dump-ast modes. */
void dump_tokens(Lexer *lexer, FILE *out);
void dump_ast(const ASTNode *root, FILE *out);

#endif /* DUMP_H */
