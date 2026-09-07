#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "ast.h"

/* Emits AMD64 AT&T assembly for the given program AST. The emitted code is
 * position-independent and uses only registers/instructions available on
 * both System V (Linux) and Microsoft x64 (Windows), so the same output can
 * be assembled with gcc on either platform.
 *
 *   windows_target != 0 : omit ELF-only directives (e.g. .note.GNU-stack)
 *                         so GNU as/MinGW on Windows accepts the output.
 *
 * Returns 0 on success, non-zero on failure. */
int codegen_generate(ASTNode *root, FILE *output, int windows_target);

#endif /* CODEGEN_H */
