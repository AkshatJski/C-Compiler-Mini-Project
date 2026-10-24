#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "ast.h"

/* Assembler flavour to emit. Instruction selection is identical on all three
 * -- the same AMD64 AT&T text assembles everywhere gcc/clang run -- but a few
 * object-format details differ, so the caller has to say where the output is
 * going:
 *
 *   TARGET_LINUX   ELF:   plain symbol names + .note.GNU-stack
 *   TARGET_WINDOWS COFF:  plain names, no ELF-only directives
 *   TARGET_DARWIN  Mach-O: a leading '_' on every external symbol
 *
 * The default is picked at build time in main.c from the host OS. */
typedef enum
{
    TARGET_LINUX = 0,
    TARGET_WINDOWS = 1,
    TARGET_DARWIN = 2
} TargetKind;

/* Emits AMD64 AT&T assembly for the given program AST. The emitted code is
 * position-independent (all symbol access is %rip-relative), so the result
 * links with the default PIE settings of the host toolchain.
 *
 * Returns 0 on success, non-zero on failure. */
int codegen_generate(ASTNode *root, FILE *output, TargetKind target);

#endif /* CODEGEN_H */
