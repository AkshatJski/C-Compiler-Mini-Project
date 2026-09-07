#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

#include "tokens.h"

typedef struct {
    const char *source;
    size_t pos;
    int line;
    int col;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token lexer_next_token(Lexer *lexer);
const char *token_type_to_string(TokenType type);

#endif /* LEXER_H */
