/* 3_optimizer.c -- the "what did the optimizer actually do?" demo.
 *
 * Compile it twice and count lines of assembly:
 *
 *   mycc 3_optimizer.c           -o build\opt.s
 *   mycc --no-opt 3_optimizer.c  -o build\plain.s
 *
 * The optimized file is roughly half the size and the program behaves
 * identically -- which is the only acceptable outcome for an optimizer, and
 * the property the test suite enforces by running every program through both
 * paths.
 *
 * Three things shrink here:
 *  - `2 + 3 * 4` is folded to 14 before code generation;
 *  - `if (0)` / `while (0)` bodies are never emitted;
 *  - `i + 1`, `i < 10`, `total * 2` keep their literal inside the
 *    instruction instead of spending a register and two stack operations.
 *
 * The program is a boring sum on purpose: the audience should watch the
 * instruction count change, not the behaviour.
 */
int main(void) {
    int total = 0;
    int i;

    /* constant folding: one literal, not three */
    total = total + (2 + 3 * 4);

    /* dead code: this body is type-checked, then never emitted */
    if (0) {
        total = total + 1000;
    }
    while (0) {
        total = total + 1000;
    }

    /* immediate operands: the hot loop */
    for (i = 0; i < 10; i = i + 1) {
        total = total + i;
    }

    total = total * 2;

    /* 14 + 45 = 59, doubled = 118 */
    return total;
}
