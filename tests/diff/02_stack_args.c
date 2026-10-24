/* Differential test 02 -- argument passing (ROADMAP Phase 4).
 *
 * Cross-checked against the host compiler: both must exit 0. This exercises
 * the one place the two 64-bit calling conventions disagree on stack layout:
 * Windows x64 passes 4 arguments in registers (rcx, rdx, r8, r9) and the rest
 * on the stack; SysV passes 6 (rdi, rsi, rdx, rcx, r8, r9). A wrong offset for
 * the first stack argument silently corrupts one value, which is exactly what
 * `expect5` below detects.
 *
 * Deliberately contains NO I/O: mycc's print_* helpers are compiler built-ins
 * that a reference compiler does not have, so a shared test must be plain C.
 * (The I/O path is covered by tests/26..28, which only mycc compiles.)
 *
 * WIDTH-AGNOSTIC: no sizeof, no byte offsets, no 32-bit wraparound -- mycc's
 * int is 64-bit while gcc's is 32.
 */

int sum5(int a, int b, int c, int d, int e) {
    return a + b + c + d + e;
}

int expect5(int a, int b, int c, int d, int e) {
    /* Position-sensitive: every argument contributes a distinct place value,
     * so swapping or dropping any one of them changes the result. */
    return a * 10000 + b * 1000 + c * 100 + d * 10 + e;
}

int sum7(int a, int b, int c, int d, int e, int f, int g) {
    return a + b + c + d + e + f + g;
}

int main(void) {
    if (sum5(10, 20, 30, 40, 50) != 150) {
        return 1;
    }
    if (expect5(1, 2, 3, 4, 5) != 12345) {
        return 2;
    }
    /* 7 arguments: 3 on the stack on Windows, 1 on the stack under SysV. */
    if (sum7(1, 2, 3, 4, 5, 6, 7) != 28) {
        return 3;
    }
    return 0;
}
