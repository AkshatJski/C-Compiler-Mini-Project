#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "codegen.h"
#include "dump.h"
#include "lexer.h"
#include "myc.h"
#include "opt.h"
#include "parser.h"
#include "semantic.h"
#include "util.h"

static void usage(void)
{
    fprintf(stderr,
            "Usage: mycc <source> [-o <output.s>] [--lang=c|myc] "
            "[--target=linux|windows|darwin] [--no-opt] [--sign] [-w]\n");
    fprintf(stderr,
            "  Compiles a C-subset program to AMD64 AT&T assembly.\n");
    fprintf(stderr,
            "  --lang=myc     parse the toy language instead of C; both\n");
    fprintf(stderr,
            "                 front ends produce the same AST, so equivalent\n");
    fprintf(stderr,
            "                 programs emit identical assembly\n");
    fprintf(stderr,
            "  --target selects the assembler flavor; defaults to the\n");
    fprintf(stderr,
            "  platform mycc was built for (auto-detected at build time).\n");
    fprintf(stderr,
            "  --dump-tokens  print the token stream, then stop\n");
    fprintf(stderr,
            "  --dump-ast     print the syntax tree (after checks), then stop\n");
    fprintf(stderr,
            "  --no-opt       disable all four optimizations: constant\n");
    fprintf(stderr,
            "                 folding, constant-condition elimination, dead\n");
    fprintf(stderr,
            "                 local stores, and temporaries in registers\n");
    fprintf(stderr,
            "  --sign         embed a provenance string in the program's data\n");
    fprintf(stderr,
            "  --version      print the compiler version and exit\n");
    fprintf(stderr,
            "  -w             suppress warnings (they never affect the exit "
            "status)\n");
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
    /* The lexer treats NUL as end-of-input. A NUL embedded in the file would
     * therefore silently truncate the translation unit, so reject it here
     * instead of compiling a prefix of the program. */
    if (memchr(buf, '\0', n) != NULL) {
        fprintf(stderr, "error: source file '%s' contains a NUL byte\n", path);
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    return buf;
}

/* Do two path spellings name the same existing file? An exact string compare
 * misses `foo.c` vs `./foo.c`, a case-insensitive Windows spelling, a hard
 * link, or a symlink. POSIX compares file identity, which catches all of those;
 * Windows canonicalizes the two paths and compares case-insensitively, which
 * covers the ordinary aliases. Missing paths simply are not the same file. */
static int same_file(const char *a, const char *b)
{
#if defined(_WIN32)
    /* _fullpath resolves relative components without requiring the file to
     * exist (the output may be new). Windows paths are case-insensitive, so
     * compare them that way. This does not resolve every possible alias
     * (short names or hard links), but it closes the ordinary destructive
     * `foo.c` vs `.\foo.c` / `FOO.C` spellings. */
    char *fa = _fullpath(NULL, a, 0);
    char *fb = _fullpath(NULL, b, 0);
    int same = fa != NULL && fb != NULL && _stricmp(fa, fb) == 0;
    free(fa);
    free(fb);
    return same;
#else
    struct stat sa;
    struct stat sb;
    if (stat(a, &sa) != 0 || stat(b, &sb) != 0)
    {
        return 0;
    }
    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
#endif
}

int main(int argc, char **argv)
{
    const char *source_path = NULL;
    const char *output_path = NULL;
    /* The host's native object format is the sensible default; --target
     * overrides it for cross-compiling the assembly text. */
#if defined(_WIN32)
    TargetKind target_kind = TARGET_WINDOWS;
#elif defined(__APPLE__)
    TargetKind target_kind = TARGET_DARWIN;
#else
    TargetKind target_kind = TARGET_LINUX;
#endif
    int dump_tokens_flag = 0;
    int dump_ast_flag = 0;
    int version_flag = 0;
    /* Phase 8 (toy language): which front end reads the source. The default is
     * C; --lang=myc selects the toy grammar. Everything after the parser is
     * shared, which is the entire point of the exercise. */
    int lang_myc = 0;

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
        } else if (strcmp(argv[i], "-w") == 0) {
            /* Warnings are advisory; this only silences the channel, it never
             * changes the exit status. */
            set_warnings_enabled(0);
        } else if (strcmp(argv[i], "--no-opt") == 0) {
            /* Phase 5: emit exactly what the pre-optimizer compiler emitted.
             * Used to compare the two back ends instruction for instruction,
             * and to bisect an optimizer bug against a known-good path. */
            g_optimize = 0;
        } else if (strcmp(argv[i], "--sign") == 0) {
            /* Phase 7: put a provenance string in the program's .rodata so
             * `strings prog.exe` can prove which compiler built it. */
            g_sign = 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            version_flag = 1;
        } else if (strncmp(argv[i], "--lang=", 7) == 0) {
            const char *lang = argv[i] + 7;
            if (strcmp(lang, "myc") == 0) {
                lang_myc = 1;
            } else if (strcmp(lang, "c") == 0) {
                lang_myc = 0;
            } else {
                fprintf(stderr,
                        "error: unknown language '%s' (use c or myc)\n", lang);
                return 1;
            }
        } else if (strncmp(argv[i], "--target=", 9) == 0) {
            const char *target_name = argv[i] + 9;
            if (strcmp(target_name, "linux") == 0) {
                target_kind = TARGET_LINUX;
            } else if (strcmp(target_name, "windows") == 0) {
                target_kind = TARGET_WINDOWS;
            } else if (strcmp(target_name, "darwin") == 0) {
                target_kind = TARGET_DARWIN;
            } else {
                fprintf(stderr,
                        "error: unknown target '%s' "
                        "(use linux, windows or darwin)\n",
                        target_name);
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

    if (version_flag) {
        /* Phase 7: one line that answers "what am I, and what can I target?"
         * without compiling anything. Kept deliberately grep-able. */
        printf("mycc 0.7 - C-subset compiler for AMD64 AT&T assembly\n");
        printf("targets: linux, windows, darwin (default: %s)\n",
               target_kind == TARGET_WINDOWS ? "windows"
               : target_kind == TARGET_DARWIN ? "darwin"
                                              : "linux");
        printf("optimizer: %s (--no-opt to disable); --sign stamps programs\n",
               g_optimize ? "on: folding, dead stores, registers" : "off");
        return 0;
    }

    if (source_path == NULL) {
        usage();
        return 1;
    }

    /* Refuse the destructive invocation `mycc foo.c -o foo.c`: the
     * temporary-file rename would replace the source with assembly. The
     * string compare covers the common typo; same_file() also catches
     * alternate spellings, POSIX links and Windows case folding. */
    if (output_path != NULL &&
        (strcmp(source_path, output_path) == 0 ||
         same_file(source_path, output_path))) {
        fprintf(stderr,
                "error: input and output are the same file '%s'\n",
                source_path);
        return 1;
    }

    error_reset();
    set_source_path(source_path);

    char *source = read_file(source_path);
    if (source == NULL) {
        return 1;
    }
    /* Phase 6: from here on a diagnostic can quote the offending line. */
    set_source_text(source);

    Lexer lexer;
    lexer_init(&lexer, source);

    if (dump_tokens_flag) {
        dump_tokens(&lexer, stdout);
        free(source);
        return error_count() > 0 ? 1 : 0;
    }

    ASTNode *ast = lang_myc ? myc_parse_program(&lexer) : parse_program(&lexer);
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

    /* Phase 5: the AST optimizer, after typing and before anything looks at the
     * tree. `--dump-ast` therefore shows the code that actually runs, which is
     * the property every other optimization in this compiler already keeps.
     * --no-opt skips it so the two paths can be compared. */
    if (g_optimize) {
        opt_run(ast);
    }

    if (dump_ast_flag) {
        dump_ast(ast, stdout);
        free(source);
        return 0;
    }

    /* Write to a temporary file and rename it into place only after the whole
     * translation unit has been generated. A failure part-way through then
     * leaves no .s behind, and an existing .s from a previous build is not
     * truncated by a compile that fails (BUG-017). */
    FILE *out = stdout;
    char *tmp_path = NULL;
    int close_out = 0;
    if (output_path != NULL) {
        size_t n = strlen(output_path) + sizeof(".mycc-tmp");
        tmp_path = xmalloc(n);
        snprintf(tmp_path, n, "%s.mycc-tmp", output_path);
        /* The source may itself be named like the temporary file (for example
         * `mycc foo.mycc-tmp -o foo.s`). Opening the temp path with "w" would
         * truncate the source we just read, so reject the collision before
         * touching the filesystem. */
        if (same_file(source_path, tmp_path)) {
            fprintf(stderr,
                    "error: input and temporary output are the same file '%s'\n",
                    source_path);
            free(tmp_path);
            free(source);
            return 1;
        }
        out = fopen(tmp_path, "w");
        if (out == NULL) {
            fprintf(stderr, "error: cannot open output file '%s'\n", tmp_path);
            free(tmp_path);
            free(source);
            return 1;
        }
        close_out = 1;
    }

    if (codegen_generate(ast, out, target_kind) != 0) {
        fprintf(stderr, "%s: compilation aborted during code generation\n",
                source_path);
        if (close_out) {
            fclose(out);
            remove(tmp_path);
        }
        free(tmp_path);
        free(source);
        return 1;
    }

    if (close_out) {
        int failed = (fflush(out) != 0); /* catch a full disk */
        if (fclose(out) != 0) {
            failed = 1;
        }
        if (!failed && rename(tmp_path, output_path) != 0) {
            /* Windows refuses to replace an existing file with rename(). */
            remove(output_path);
            failed = (rename(tmp_path, output_path) != 0);
        }
        if (failed) {
            fprintf(stderr, "error: cannot write output file '%s'\n",
                    output_path);
            remove(tmp_path);
            free(tmp_path);
            free(source);
            return 1;
        }
    }
    free(tmp_path);
    free(source);
    return 0;
}
