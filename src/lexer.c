#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "util.h"

/* `#include`, `#define` and friends belong to the preprocessor, which mycc
 * does not implement. Every line that starts with '#' in trivia position is
 * therefore skipped whole and reported *once* per compilation: reporting it
 * per line turns a single `#include` into a wall of identical errors. */
static int g_directive_reported = 0;

void lexer_init(Lexer *lexer, const char *source)
{
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->col = 1;
    g_directive_reported = 0;

    /* A UTF-8 byte-order mark is invisible but would otherwise be lexed as
     * two stray high-bit characters (BUG-011). Editors on Windows write it
     * by default, so skipping it here is worth the four lines. */
    if ((unsigned char)source[0] == 0xEF &&
        (unsigned char)source[1] == 0xBB &&
        (unsigned char)source[2] == 0xBF)
    {
        lexer->pos = 3;
    }
}

static int at_end(const Lexer *lexer)
{
    return lexer->source[lexer->pos] == '\0';
}

static char peek(const Lexer *lexer)
{
    return lexer->source[lexer->pos];
}

static char peek_next(const Lexer *lexer)
{
    if (lexer->source[lexer->pos + 1] == '\0')
    {
        return '\0';
    }
    return lexer->source[lexer->pos + 1];
}

static char advance(Lexer *lexer)
{
    char c = lexer->source[lexer->pos++];
    if (c == '\n')
    {
        lexer->line++;
        lexer->col = 1;
    }
    else
    {
        lexer->col++;
    }
    return c;
}

static Token make_token(TokenType type, char *value, int line, int col)
{
    Token token;
    token.type = type;
    token.value = value;
    token.line = line;
    token.col = col;
    token.length = 0; /* only string literals set this (BUG-023) */
    return token;
}

static void skip_trivia(Lexer *lexer)
{
    for (;;)
    {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            advance(lexer);
        }
        else if (c == '/' && peek_next(lexer) == '/')
        {
            while (!at_end(lexer) && peek(lexer) != '\n')
            {
                advance(lexer);
            }
        }
        else if (c == '/' && peek_next(lexer) == '*')
        {
            int start_line = lexer->line;
            int start_col = lexer->col;
            advance(lexer); /* '/' */
            advance(lexer); /* '*' */
            int closed = 0;
            while (!at_end(lexer))
            {
                if (peek(lexer) == '*' && peek_next(lexer) == '/')
                {
                    advance(lexer);
                    advance(lexer);
                    closed = 1;
                    break;
                }
                advance(lexer);
            }
            if (!closed)
            {
                report_error(start_line, start_col, "unterminated block comment");
                return;
            }
        }
        else if (c == '#')
        {
            /* Preprocessor directive: not supported, but the *reason* matters
             * more than the symptom, and one report per file is enough. The
             * whole physical line is dropped so the parser sees clean input
             * (BUG-012). */
            if (!g_directive_reported)
            {
                g_directive_reported = 1;
                report_error(lexer->line, lexer->col,
                             "preprocessor directives are not supported "
                             "(mycc has no #include/#define; write "
                             "self-contained code)");
            }
            while (!at_end(lexer) && peek(lexer) != '\n')
            {
                advance(lexer);
            }
        }
        else
        {
            return;
        }
    }
}

static Token lex_ident_or_keyword(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->col;
    size_t start = lexer->pos;
    while (isalnum((unsigned char)peek(lexer)) || peek(lexer) == '_')
    {
        advance(lexer);
    }
    size_t len = lexer->pos - start;
    char *text = xmalloc(len + 1);
    memcpy(text, lexer->source + start, len);
    text[len] = '\0';

    struct Keyword
    {
        const char *word;
        TokenType type;
    };
    static const struct Keyword keywords[] = {
        {"int", TOKEN_KEYWORD_INT},
        {"char", TOKEN_KEYWORD_CHAR},
        {"signed", TOKEN_KEYWORD_SIGNED},
        {"unsigned", TOKEN_KEYWORD_UNSIGNED},
        {"void", TOKEN_KEYWORD_VOID},
        {"if", TOKEN_KEYWORD_IF},
        {"else", TOKEN_KEYWORD_ELSE},
        {"while", TOKEN_KEYWORD_WHILE},
        {"do", TOKEN_KEYWORD_DO},
        {"for", TOKEN_KEYWORD_FOR},
        {"switch", TOKEN_KEYWORD_SWITCH},
        {"case", TOKEN_KEYWORD_CASE},
        {"default", TOKEN_KEYWORD_DEFAULT},
        {"break", TOKEN_KEYWORD_BREAK},
        {"continue", TOKEN_KEYWORD_CONTINUE},
        {"return", TOKEN_KEYWORD_RETURN},
        {"struct", TOKEN_KEYWORD_STRUCT},
        {"extern", TOKEN_KEYWORD_EXTERN},
    };

    TokenType type = TOKEN_IDENT;
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
    {
        if (strcmp(text, keywords[i].word) == 0)
        {
            type = keywords[i].type;
            break;
        }
    }
    return make_token(type, text, start_line, start_col);
}

static int is_num_suffix(char c)
{
    return c == 'u' || c == 'U' || c == 'l' || c == 'L';
}

static Token lex_number(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->col;
    size_t start = lexer->pos;

    if (peek(lexer) == '0' &&
        (peek_next(lexer) == 'x' || peek_next(lexer) == 'X'))
    {
        advance(lexer); /* '0' */
        advance(lexer); /* 'x' */
        if (!isxdigit((unsigned char)peek(lexer)))
        {
            report_error(start_line, start_col,
                         "malformed hexadecimal literal");
        }
        while (isxdigit((unsigned char)peek(lexer)))
        {
            advance(lexer);
        }
    }
    else
    {
        /* A leading 0 with more digits means octal, so "08" and "09" are
         * errors rather than silently becoming 0 (BUG-008). Validate the
         * digits we just consumed; report at the offending column. */
        int octal = peek(lexer) == '0';
        while (isdigit((unsigned char)peek(lexer)))
        {
            advance(lexer);
        }
        if (octal)
        {
            for (size_t p = start + 1; p < lexer->pos; p++)
            {
                if (lexer->source[p] > '7')
                {
                    report_error(start_line, start_col + (int)(p - start),
                                 "invalid digit '%c' in octal constant",
                                 lexer->source[p]);
                    break;
                }
            }
        }
    }
    /* Optional integer suffix (U / L / UL / LL / ULL in any reasonably
     * ordered spelling); the suffix is dropped from the stored text. */
    size_t digits_end = lexer->pos;
    while (is_num_suffix(peek(lexer)))
    {
        advance(lexer);
    }

    size_t len = digits_end - start;
    char *text = xmalloc(len + 1);
    memcpy(text, lexer->source + start, len);
    text[len] = '\0';
    return make_token(TOKEN_INT_LIT, text, start_line, start_col);
}

/* Decode one escape sequence after the backslash; returns the byte value
 * and advances past it. */
static int read_escape(Lexer *lexer)
{
    if (at_end(lexer))
    {
        /* The literal is unterminated at end of file; the caller reports it
         * and the stray byte must not be consumed (it is the terminator). */
        return 0;
    }
    char c = advance(lexer);
    switch (c)
    {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'v':
        return '\v';
    case '\\':
        return '\\';
    case '\'':
        return '\'';
    case '"':
        return '"';
    case '?':
        /* C99 6.4.4.4: `\?` exists so a character sequence can avoid forming
         * a trigraph. Trigraphs are not implemented, but the escape is still
         * ordinary C and denotes '?'. */
        return '?';
    case 'x':
    {
        int value = 0;
        int digits = 0;
        int out_of_range = 0;
        while (isxdigit((unsigned char)peek(lexer)))
        {
            char h = advance(lexer);
            int digit = isdigit((unsigned char)h)
                            ? h - '0'
                            : (tolower((unsigned char)h) - 'a' + 10);
            /* A byte is the only thing an escape can denote. Stop growing the
             * value once it has overflowed so a long digit sequence cannot
             * overflow `int`, but remember that it was too large. */
            if (value > 0xFF)
            {
                out_of_range = 1;
            }
            else
            {
                value = value * 16 + digit;
                if (value > 0xFF)
                {
                    out_of_range = 1;
                }
            }
            digits++;
        }
        if (digits == 0)
        {
            report_error(lexer->line, lexer->col, "\\x used with no digits");
            return 0;
        }
        if (out_of_range)
        {
            report_error(lexer->line, lexer->col,
                         "hex escape value is too large for a byte");
        }
        return value & 0xFF;
    }
    default:
        /* Octal escapes: one to three digits, e.g. \0 (NUL), \101 ('A'),
         * \377. `\0` is just the one-digit case, so it needs no separate arm
         * (BUG-022: this was missing, and only became visible once string
         * literals were emitted as real data). */
        if (c >= '0' && c <= '7')
        {
            int value = c - '0';
            int digits = 1;
            while (digits < 3 && peek(lexer) >= '0' && peek(lexer) <= '7')
            {
                value = value * 8 + (advance(lexer) - '0');
                digits++;
            }
            if (value > 0xFF)
            {
                report_error(lexer->line, lexer->col,
                             "octal escape value is too large for a byte");
            }
            return value & 0xFF;
        }
        report_error(lexer->line, lexer->col, "unknown escape sequence '\\%c'", c);
        return c;
    }
}

static Token lex_char_literal(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->col;
    advance(lexer); /* opening ' */

    int value;
    if (at_end(lexer))
    {
        report_error(start_line, start_col, "unterminated character literal");
        value = 0;
    }
    else if (peek(lexer) == '\'')
    {
        report_error(start_line, start_col, "empty character literal");
        value = 0;
    }
    else if (peek(lexer) == '\\')
    {
        advance(lexer); /* backslash */
        value = read_escape(lexer);
    }
    else
    {
        value = (unsigned char)advance(lexer);
    }

    if (!at_end(lexer) && peek(lexer) == '\'')
    {
        advance(lexer); /* closing ' */
    }
    else
    {
        report_error(start_line, start_col, "unterminated character literal");
    }

    /* A character constant has type int, but this compiler's `char` is signed.
     * A single byte with the high bit set therefore denotes the corresponding
     * negative value: '\xFF' is -1, not 255. String literals keep the raw byte
     * and must not be sign-extended. */
    if (value >= 0x80 && value <= 0xFF)
    {
        value -= 0x100;
    }

    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    return make_token(TOKEN_CHAR_LIT, xstrdup(text), start_line, start_col);
}

/* String literals (ROADMAP Phase 4). The decoded bytes live in the token and
 * are later emitted into .rodata, so `"hello\n"` is a real `char *` a program
 * can hand to printf.
 *
 * Escapes are decoded here, reusing read_escape(), so exactly one function
 * knows what a backslash means and the assembler only ever sees bytes. Before
 * this, an unsupported literal was skipped and reported (BUG-013); that
 * rejection is gone now that strings are supported. */
static Token lex_string_literal(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->col;
    advance(lexer); /* opening '"' */

    size_t cap = 32;
    size_t len = 0;
    char *buf = xmalloc(cap);

    int closed = 0;
    while (!at_end(lexer) && peek(lexer) != '\n')
    {
        int ch;
        if (peek(lexer) == '"')
        {
            advance(lexer);
            closed = 1;
            break;
        }
        if (peek(lexer) == '\\')
        {
            advance(lexer); /* the backslash itself */
            ch = read_escape(lexer);
        }
        else
        {
            ch = (unsigned char)advance(lexer);
        }
        if (len + 1 >= cap)
        {
            cap *= 2;
            buf = xrealloc(buf, cap);
        }
        buf[len++] = (char)ch;
    }

    if (!closed)
    {
        report_error(start_line, start_col, "unterminated string literal");
    }

    buf[len] = '\0';

    /* Keep the byte count: the literal may legitimately contain NUL bytes,
     * which strlen() could not report (BUG-023). */
    Token token = make_token(TOKEN_STRING, buf, start_line, start_col);
    token.length = (int)len;
    return token;
}

Token lexer_next_token(Lexer *lexer)
{
    /* A string literal produces no token: drop it and keep looking. Doing
     * this in a loop (rather than by recursion) keeps deeply string-heavy
     * files from growing the C stack. */
    for (;;)
    {
        skip_trivia(lexer);
        if (at_end(lexer))
        {
            return make_token(TOKEN_EOF, NULL, lexer->line, lexer->col);
        }
        break;
    }

    int line = lexer->line;
    int col = lexer->col;
    char c = peek(lexer);

    if (isalpha((unsigned char)c) || c == '_')
    {
        return lex_ident_or_keyword(lexer);
    }
    if (isdigit((unsigned char)c))
    {
        return lex_number(lexer);
    }
    if (c == '\'')
    {
        return lex_char_literal(lexer);
    }
    if (c == '"')
    {
        return lex_string_literal(lexer);
    }

    advance(lexer);
    switch (c)
    {
    case '+':
        if (peek(lexer) == '+')
        {
            advance(lexer);
            return make_token(TOKEN_PLUS_PLUS, NULL, line, col);
        }
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_PLUS_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_PLUS, NULL, line, col);
    case '-':
        if (peek(lexer) == '-')
        {
            advance(lexer);
            return make_token(TOKEN_MINUS_MINUS, NULL, line, col);
        }
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_MINUS_EQUAL, NULL, line, col);
        }
        if (peek(lexer) == '>')
        {
            advance(lexer);
            return make_token(TOKEN_ARROW, NULL, line, col);
        }
        return make_token(TOKEN_MINUS, NULL, line, col);
    case '*':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_STAR_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_STAR, NULL, line, col);
    case '/':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_SLASH_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_SLASH, NULL, line, col);
    case '%':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_PERCENT_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_PERCENT, NULL, line, col);
    case '&':
        if (peek(lexer) == '&')
        {
            advance(lexer);
            return make_token(TOKEN_AMPERSAND_AMPERSAND, NULL, line, col);
        }
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_AMPERSAND_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_AMPERSAND, NULL, line, col);
    case '|':
        if (peek(lexer) == '|')
        {
            advance(lexer);
            return make_token(TOKEN_PIPE_PIPE, NULL, line, col);
        }
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_PIPE_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_PIPE, NULL, line, col);
    case '^':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_CARET_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_CARET, NULL, line, col);
    case '~':
        return make_token(TOKEN_TILDE, NULL, line, col);
    case '<':
        if (peek(lexer) == '<')
        {
            advance(lexer);
            if (peek(lexer) == '=')
            {
                advance(lexer);
                return make_token(TOKEN_LESS_LESS_EQUAL, NULL, line, col);
            }
            return make_token(TOKEN_LESS_LESS, NULL, line, col);
        }
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_LESS_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_LESS, NULL, line, col);
    case '>':
        if (peek(lexer) == '>')
        {
            advance(lexer);
            if (peek(lexer) == '=')
            {
                advance(lexer);
                return make_token(TOKEN_GREATER_GREATER_EQUAL, NULL, line, col);
            }
            return make_token(TOKEN_GREATER_GREATER, NULL, line, col);
        }
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_GREATER_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_GREATER, NULL, line, col);
    case '(':
        return make_token(TOKEN_LPAREN, NULL, line, col);
    case ')':
        return make_token(TOKEN_RPAREN, NULL, line, col);
    case '{':
        return make_token(TOKEN_LBRACE, NULL, line, col);
    case '}':
        return make_token(TOKEN_RBRACE, NULL, line, col);
    case ',':
        return make_token(TOKEN_COMMA, NULL, line, col);
    case ';':
        return make_token(TOKEN_SEMICOLON, NULL, line, col);
    case '?':
        return make_token(TOKEN_QUESTION, NULL, line, col);
    case ':':
        return make_token(TOKEN_COLON, NULL, line, col);
    case '=':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_EQUALS_EQUALS, NULL, line, col);
        }
        return make_token(TOKEN_ASSIGN, NULL, line, col);
    case '!':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(TOKEN_BANG_EQUALS, NULL, line, col);
        }
        return make_token(TOKEN_BANG, NULL, line, col);
    case '[':
        return make_token(TOKEN_LBRACKET, NULL, line, col);
    case ']':
        return make_token(TOKEN_RBRACKET, NULL, line, col);
    case '.':
        return make_token(TOKEN_DOT, NULL, line, col);

    default:
        report_error(line, col, "unexpected character '%c'", c);
        return make_token(TOKEN_EOF, NULL, line, col);
    }
}

const char *token_type_to_string(TokenType type)
{
    switch (type)
    {
    case TOKEN_EOF:
        return "end of file";
    case TOKEN_INT_LIT:
        return "integer literal";
    case TOKEN_CHAR_LIT:
        return "character literal";
    case TOKEN_STRING:
        return "string literal";
    case TOKEN_IDENT:
        return "identifier";
    case TOKEN_KEYWORD_INT:
        return "'int'";
    case TOKEN_KEYWORD_CHAR:
        return "'char'";
    case TOKEN_KEYWORD_SIGNED:
        return "'signed'";
    case TOKEN_KEYWORD_UNSIGNED:
        return "'unsigned'";
    case TOKEN_KEYWORD_VOID:
        return "'void'";
    case TOKEN_KEYWORD_IF:
        return "'if'";
    case TOKEN_KEYWORD_ELSE:
        return "'else'";
    case TOKEN_KEYWORD_WHILE:
        return "'while'";
    case TOKEN_KEYWORD_DO:
        return "'do'";
    case TOKEN_KEYWORD_FOR:
        return "'for'";
    case TOKEN_KEYWORD_SWITCH:
        return "'switch'";
    case TOKEN_KEYWORD_CASE:
        return "'case'";
    case TOKEN_KEYWORD_DEFAULT:
        return "'default'";
    case TOKEN_KEYWORD_BREAK:
        return "'break'";
    case TOKEN_KEYWORD_CONTINUE:
        return "'continue'";
    case TOKEN_KEYWORD_RETURN:
        return "'return'";
    case TOKEN_PLUS:
        return "'+'";
    case TOKEN_MINUS:
        return "'-'";
    case TOKEN_STAR:
        return "'*'";
    case TOKEN_SLASH:
        return "'/'";
    case TOKEN_PERCENT:
        return "'%'";
    case TOKEN_PLUS_PLUS:
        return "'++'";
    case TOKEN_MINUS_MINUS:
        return "'--'";
    case TOKEN_PLUS_EQUAL:
        return "'+='";
    case TOKEN_MINUS_EQUAL:
        return "'-='";
    case TOKEN_STAR_EQUAL:
        return "'*='";
    case TOKEN_SLASH_EQUAL:
        return "'/='";
    case TOKEN_PERCENT_EQUAL:
        return "'%='";
    case TOKEN_AMPERSAND_EQUAL:
        return "'&='";
    case TOKEN_PIPE_EQUAL:
        return "'|='";
    case TOKEN_CARET_EQUAL:
        return "'^='";
    case TOKEN_LESS_LESS_EQUAL:
        return "'<<='";
    case TOKEN_GREATER_GREATER_EQUAL:
        return "'>>='";
    case TOKEN_AMPERSAND:
        return "'&'";
    case TOKEN_AMPERSAND_AMPERSAND:
        return "'&&'";
    case TOKEN_PIPE:
        return "'|'";
    case TOKEN_PIPE_PIPE:
        return "'||'";
    case TOKEN_CARET:
        return "'^'";
    case TOKEN_TILDE:
        return "'~'";
    case TOKEN_LESS_LESS:
        return "'<<'";
    case TOKEN_GREATER_GREATER:
        return "'>>'";
    case TOKEN_EQUALS_EQUALS:
        return "'=='";
    case TOKEN_BANG_EQUALS:
        return "'!='";
    case TOKEN_BANG:
        return "'!'";
    case TOKEN_LESS:
        return "'<'";
    case TOKEN_LESS_EQUAL:
        return "'<='";
    case TOKEN_GREATER:
        return "'>'";
    case TOKEN_GREATER_EQUAL:
        return "'>='";
    case TOKEN_QUESTION:
        return "'?'";
    case TOKEN_COLON:
        return "':'";
    case TOKEN_ASSIGN:
        return "'='";
    case TOKEN_LPAREN:
        return "'('";
    case TOKEN_RPAREN:
        return "')'";
    case TOKEN_LBRACE:
        return "'{'";
    case TOKEN_RBRACE:
        return "'}'";
    case TOKEN_COMMA:
        return "','";
    case TOKEN_SEMICOLON:
        return "';'";
    case TOKEN_LBRACKET:
        return "'['";
    case TOKEN_RBRACKET:
        return "']'";
    case TOKEN_KEYWORD_STRUCT:
        return "'struct'";
    case TOKEN_KEYWORD_EXTERN:
        return "'extern'";
    case TOKEN_DOT:
        return "'.'";
    case TOKEN_ARROW:
        return "'->'";
    }

    return "unknown token";
}
