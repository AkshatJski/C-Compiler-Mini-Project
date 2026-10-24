#ifndef MYC_H
#define MYC_H

#include "ast.h"
#include "lexer.h"

/* Toy-language front end (--lang=myc).
 *
 * `myc` is deliberately NOT a C dialect with new syntax sugar: it is a separate
 * language that happens to compile through the same AST. The lexer is reused
 * because tokens are not the interesting part; the parser is not, because the
 * grammar and the tree it builds are what distinguish the two languages.
 *
 * The payoff is a demonstration that cannot be faked: the C source and the myc
 * source of the same program produce *byte-identical assembly*, because they
 * produce the same tree.
 *
 * Returns an AST_PROGRAM, or NULL after reporting at least one error. The
 * caller treats NULL and error_count() > 0 the same way it does for the C
 * parser, so the rest of the pipeline is unaware which front end ran.
 */
ASTNode *myc_parse_program(Lexer *lexer);

#endif /* MYC_H */
