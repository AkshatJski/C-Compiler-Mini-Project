#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

ASTNode *parse_program(Lexer *lexer);

#endif /* PARSER_H */
