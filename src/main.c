#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "dump.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "util.h"

static void usage(void)
{
    fprintf(stderr,
            "Usage: mycc <source.c> [-o <output.s>] [--target=linux|windows]\n");
    fprintf(stderr,
            "  Compiles a C-subset program to AMD64 AT&T assembly.\n");
    fprintf(stderr,
            "  --target selects the assembler flavor; defaults to the\n");
    fprintf(stderr,
            "  platform mycc was built for (auto-detected at build time).\n");
    fprintf(stderr,
            "  --dump-tokens  print the token stream, then stop\n");
    fprintf(stderr,
            "  --dump-ast     print the syntax tree (after checks), then stop\n");
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "error: cannot open source file '%s'\n", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = xmalloc((size_t)size + 1);
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (n != (size_t)size) {
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    return buf;
}

int main(int argc, char **argv)
{
    const char *source_path = NULL;
    const char *output_path = NULL;
    int windows_target = 0;
    int dump_tokens_flag = 0;
    int dump_ast_flag = 0;
#ifdef _WIN32
    windows_target = 1;
#endif

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: '-o' requires an argument\n");
                usage();
                return 1;
            }
            output_path = argv[++i];
        } else if (strcmp(argv[i], "--dump-tokens") == 0) {
            dump_tokens_flag = 1;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            dump_ast_flag = 1;
        } else if (strncmp(argv[i], "--target=", 9) == 0) {
            const char *target = argv[i] + 9;
            if (strcmp(target, "linux") == 0) {
                windows_target = 0;
            } else if (strcmp(target, "windows") == 0) {
                windows_target = 1;
            } else {
                fprintf(stderr, "error: unknown target '%s' (use linux or windows)\n",
                        target);
                usage();
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            usage();
            return 1;
        } else if (source_path == NULL) {
            source_path = argv[i];
        } else {
            fprintf(stderr, "error: unexpected extra argument '%s'\n", argv[i]);
            usage();
            return 1;
        }
    }

    if (source_path == NULL) {
        usage();
        return 1;
    }

    error_reset();
    set_source_path(source_path);

    char *source = read_file(source_path);
    if (source == NULL) {
        return 1;
    }

    Lexer lexer;
    lexer_init(&lexer, source);

    if (dump_tokens_flag) {
        dump_tokens(&lexer, stdout);
        free(source);
        return error_count() > 0 ? 1 : 0;
    }

    ASTNode *ast = parse_program(&lexer);
    if (ast == NULL || error_count() > 0) {
        fprintf(stderr, "%s: compilation aborted during parsing\n", source_path);
        free(source);
        return 1;
    }

    if (semantic_analyze(ast) != 0) {
        fprintf(stderr, "%s: compilation aborted during semantic analysis\n",
                source_path);
        free(source);
        return 1;
    }

    if (dump_ast_flag) {
        dump_ast(ast, stdout);
        free(source);
        return 0;
    }

    FILE *out = stdout;
    int close_out = 0;
    if (output_path != NULL) {
        out = fopen(output_path, "w");
        if (out == NULL) {
            fprintf(stderr, "error: cannot open output file '%s'\n", output_path);
            free(source);
            return 1;
        }
        close_out = 1;
    }

    if (codegen_generate(ast, out, windows_target) != 0) {
        fprintf(stderr, "%s: compilation aborted during code generation\n",
                source_path);
        if (close_out) {
            fclose(out);
        }
        free(source);
        return 1;
    }

    if (close_out) {
        fclose(out);
    }
    free(source);
    return 0;
}
