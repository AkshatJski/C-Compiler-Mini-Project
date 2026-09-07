#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"

/* Runs the full semantic analysis pipeline (declaration pass, name
 * resolution, duplicate detection, and stack-offset assignment).
 * Returns 0 on success, non-zero on failure. */
int semantic_analyze(ASTNode *root);

#endif /* SEMANTIC_H */
