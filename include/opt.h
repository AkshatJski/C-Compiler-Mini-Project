#ifndef OPT_H
#define OPT_H

#include "ast.h"

/* The AST optimizer, run between semantic analysis and code generation.
 *
 * It sits *after* semantic analysis on purpose: by then every AST_VAR_REF has a
 * stack_offset and every expression has a type, so the pass never has to guess
 * what a name means. It sits *before* code generation because code generation
 * should only ever see the tree that will actually run -- the same claim
 * `--dump-ast` has always made.
 *
 * It never adds nodes, never changes a type, and never changes a frame size: it
 * only deletes stores whose value nobody can observe. That restriction keeps the
 * pass provable in a paragraph -- see opt.c.
 *
 * Gated on --no-opt by the caller (main.c), so the unoptimized path stays
 * exactly the pre-Phase-5 compiler.
 */
void opt_run(ASTNode *program);

#endif /* OPT_H */
