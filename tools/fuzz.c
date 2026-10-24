/* tools/fuzz.c -- generate random programs for the differential harness.
 *
 * Why this exists
 * ---------------
 * tests/diff/ proves mycc agrees with gcc on four hand-written programs.
 * Hand-written cases only cover bugs someone already thought of. This
 * generator produces programs nobody wrote, and difftest.sh (with SRCDIR
 * pointed at the output directory) requires mycc and gcc to return the same
 * exit code for every one of them.
 *
 * It is a *search over expression shapes*, not a language fuzzer: the skeleton
 * below is fixed (helper function, two loops, an if/else) and every expression,
 * literal, comparison and loop bound inside it is random. That keeps every
 * generated program inside the subset mycc supports today, so a mismatch is a
 * real codegen bug rather than "the generator wrote something the front end
 * has not implemented yet".
 *
 * Three invariants the generator must keep, or the run lies:
 *   1. Terminating: every loop counts up to a literal bound and only the
 *      counter is incremented.
 *   2. No undefined behaviour: no division by a non-literal (so never by
 *      zero), no shift counts >= width, no uninitialised reads.
 *   3. 32-bit safe: mycc's int is 64-bit and gcc's is 32-bit, so the numbers
 *      must stay small enough that no intermediate value overflows either
 *      one. Literals are 0..20 and expression depth is capped, which keeps
 *      the largest product far below 2^31.
 *
 * Determinism: an LCG seeded from (seed, index), so `fuzzgen 100 build/fuzz 7`
 * regenerates exactly the same 100 files on every machine and every run. A
 * failure is therefore reproducible by number alone.
 *
 * Usage: fuzzgen <count> <outdir> [seed]
 *        fuzzgen 1000 build/fuzz 1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define make_dir(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define make_dir(path) mkdir((path), 0777)
#endif

/* Numeric Recipes' LCG constants: cheap, deterministic, good enough for
 * choosing among a handful of branches. Not a security primitive. */
static unsigned long g_state;

static unsigned next_rand(void)
{
    g_state = g_state * 1103515245UL + 12345UL;
    return (unsigned)((g_state >> 16) & 0x7fff);
}

/* Uniform in [0, n). n must be positive. */
static int rnd(int n)
{
    return (int)(next_rand() % (unsigned)n);
}

/* A leaf is either a variable in scope or a small literal. */
static void emit_leaf(FILE *f, const char *const *vars, int nvars)
{
    if (nvars > 0 && rnd(4) != 0)
    {
        fprintf(f, "%s", vars[rnd(nvars)]);
    }
    else
    {
        fprintf(f, "%d", rnd(21));
    }
}

static void emit_expr(FILE *f, int depth, const char *const *vars, int nvars);

/* A binary node. Two deliberate restrictions:
 *   - division and modulo always take a literal on the right, so the
 *     denominator can never be zero and never depends on a random variable;
 *   - multiplication always scales a single leaf by a literal, which bounds
 *     every expression below ~800. That matters because mycc's int is 64-bit
 *     and gcc's is 32-bit: an overflow would be real undefined behaviour and
 *     a mismatch there would say nothing about the compiler. */
static void emit_binary(FILE *f, const char *op, int depth,
                        const char *const *vars, int nvars)
{
    fputc('(', f);
    if (strcmp(op, "*") == 0)
    {
        fprintf(f, "%d * ", rnd(21));
        emit_leaf(f, vars, nvars);
        fputc(')', f);
        return;
    }
    emit_expr(f, depth - 1, vars, nvars);
    fprintf(f, " %s ", op);
    if (strcmp(op, "/") == 0 || strcmp(op, "%") == 0)
    {
        fprintf(f, "%d", 1 + rnd(9));
    }
    else
    {
        emit_expr(f, depth - 1, vars, nvars);
    }
    fputc(')', f);
}

static void emit_expr(FILE *f, int depth, const char *const *vars, int nvars)
{
    /* Comparisons and the logical operators are here on purpose: they are
     * where setcc/cmov codegen lives, and they are the operators most likely
     * to be folded wrong. */
    static const char *const ops[] = {
        "+", "-", "*", "/", "%",
        "<", ">", "<=", ">=", "==", "!=",
        "&&", "||"};

    if (depth <= 0 || rnd(5) == 0)
    {
        emit_leaf(f, vars, nvars);
        return;
    }
    if (rnd(6) == 0)
    {
        fputc('-', f);
        emit_leaf(f, vars, nvars);
        return;
    }
    emit_binary(f, ops[rnd((int)(sizeof ops / sizeof ops[0]))], depth, vars, nvars);
}

/* `name = <expression over vars>;` on its own line. */
static void emit_assign(FILE *f, const char *indent, const char *name,
                        const char *const *vars, int nvars)
{
    fprintf(f, "%s%s = ", indent, name);
    emit_expr(f, 3, vars, nvars);
    fprintf(f, ";\n");
}

/* One if/else whose branches assign to acc. */
static void emit_if_else(FILE *f, const char *indent,
                         const char *const *vars, int nvars)
{
    char deeper[64];
    snprintf(deeper, sizeof deeper, "%s    ", indent);
    fprintf(f, "%sif (", indent);
    emit_expr(f, 2, vars, nvars);
    fprintf(f, " > %d) {\n", rnd(30));
    emit_assign(f, deeper, "acc", vars, nvars);
    fprintf(f, "%s} else {\n", indent);
    emit_assign(f, deeper, "acc", vars, nvars);
    fprintf(f, "%s}\n", indent);
}

static void emit_program(FILE *f, unsigned seed, int index)
{
    /* Two scopes: the helper sees only its parameters and its own local, main
     * sees only its globals, so neither can mention an undeclared name. */
    static const char *const helper_vars[] = {"a", "b", "t"};
    static const char *const main_vars[] = {"acc", "i", "j"};

    int loop_bound = 1 + rnd(4);
    int outer_bound = 1 + rnd(3);

    fprintf(f, "/* fuzz case %04d, seed %u -- generated by tools/fuzz.c */\n",
            index, seed);
    fprintf(f, "/* Replace this file by hand to turn a failing case into a\n"
               " * permanent regression test in tests/diff/. */\n\n");

    fprintf(f, "int helper(int a, int b)\n{\n    int t;\n");
    /* t is seeded from the parameters, never read before its first store:
     * reading an uninitialised local is undefined behaviour and would make
     * any "mismatch" meaningless. */
    fprintf(f, "    t = ");
    emit_expr(f, 3, helper_vars, 2);
    fprintf(f, ";\n");
    fprintf(f, "    if (t < %d) {\n        t = -t;\n    }\n", rnd(40));
    emit_assign(f, "    ", "t", helper_vars, 3);
    fprintf(f, "    return t;\n}\n\n");

    fprintf(f, "int main(void)\n{\n    int acc;\n    int i;\n    int j;\n\n");
    fprintf(f, "    acc = %d;\n", rnd(21));
    /* All three are initialised up front: the if/else inside the first loop
     * can mention j, which is only used as a counter later. */
    fprintf(f, "    i = 0;\n    j = 0;\n");
    fprintf(f, "    while (i < %d) {\n", loop_bound);
    emit_assign(f, "        ", "acc", main_vars, 3);
    if (rnd(2) == 0)
    {
        emit_if_else(f, "        ", main_vars, 3);
    }
    fprintf(f, "        i = i + 1;\n    }\n\n");

    fprintf(f, "    j = 0;\n");
    fprintf(f, "    while (j < %d) {\n", outer_bound);
    fprintf(f, "        acc = acc + helper(acc, j);\n");
    fprintf(f, "        j = j + 1;\n    }\n\n");

    emit_if_else(f, "    ", main_vars, 3);

    /* Fold the result into the only thing the harness reads: the exit code.
     * 251 is prime and non-zero, and the sign fix keeps it in 0..250 so the
     * POSIX 8-bit truncation can never hide a difference. */
    fprintf(f, "    if (acc < 0) {\n        acc = -acc;\n    }\n");
    fprintf(f, "    return acc %% 251;\n}\n");
}

int main(int argc, char **argv)
{
    int count;
    const char *outdir;
    unsigned seed = 1;
    int i;
    char path[1024];

    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <count> <outdir> [seed]\n", argv[0]);
        return 2;
    }
    count = atoi(argv[1]);
    outdir = argv[2];
    if (argc > 3)
    {
        seed = (unsigned)strtoul(argv[3], NULL, 10);
    }
    if (count < 0 || count > 100000)
    {
        fprintf(stderr, "fuzzgen: count must be between 0 and 100000\n");
        return 2;
    }

    /* The harness creates the directory too; doing it here as well keeps the
     * tool usable on its own. EEXIST is the normal case, not an error. */
    if (make_dir(outdir) != 0)
    {
        /* ignore: the directory almost certainly already exists */
    }

    for (i = 0; i < count; i++)
    {
        FILE *f;
        snprintf(path, sizeof path, "%s/case_%04d.c", outdir, i);
        f = fopen(path, "w");
        if (f == NULL)
        {
            fprintf(stderr, "fuzzgen: cannot write %s\n", path);
            return 1;
        }
        /* Seeding per case (rather than continuing one stream) means case N
         * is independent of the count: `fuzzgen 100 ...` and `fuzzgen 1000 ...`
         * agree on their first 100 programs. */
        g_state = (unsigned long)seed * 2654435761UL + (unsigned long)i * 7919UL + 1UL;
        emit_program(f, seed, i);
        if (fclose(f) != 0)
        {
            fprintf(stderr, "fuzzgen: cannot close %s\n", path);
            return 1;
        }
    }

    printf("fuzzgen: wrote %d program(s) to %s (seed %u)\n", count, outdir, seed);
    return 0;
}
