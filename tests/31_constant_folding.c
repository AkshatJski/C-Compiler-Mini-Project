/* 31_constant_folding.c
 * ROADMAP Phase 5: constant folding.
 * EXPECT: 0
 *
 * Every check below is false if folding computes the wrong value, and the whole
 * program is still the same program under --no-opt, which is where the real
 * invariant lives: the optimizer must never change what a program does.
 *
 * Deliberately 64-bit-value safe: mycc's `int` is 64-bit, so nothing here may
 * depend on 32-bit overflow.
 */

int main(void) {
    /* Precedence survives folding: 2 + 3 * 4 is 14, not 20. */
    if (2 + 3 * 4 != 14) return 1;
    if ((2 + 3) * 4 != 20) return 2;

    /* Folded through an arbitrary depth: the parent sees the child's value. */
    if (1 + 2 + 3 + 4 + 5 != 15) return 3;
    if ((1 + 2) * (3 + 4) != 21) return 4;

    /* C's truncation-toward-zero, which is what the runtime does too. */
    if (7 / 2 != 3) return 5;
    if ((-7) / 2 != -3) return 6;
    if ((-7) % 2 != -1) return 7;

    /* Bitwise and shifts. */
    if ((1 << 10) != 1024) return 8;
    if ((~0) != -1) return 9;
    if ((12 & 10) != 8) return 10;
    if ((12 | 10) != 14) return 11;
    if ((12 ^ 10) != 6) return 12;

    /* Logical operators fold to 0/1, not to the operand. */
    if ((3 && 5) != 1) return 13;
    if ((0 || 0) != 0) return 14;
    if ((0 || 7) != 1) return 15;

    /* Comparisons. */
    if ((2 < 3) != 1) return 16;
    if ((3 <= 3) != 1) return 17;
    if ((2 == 3) != 0) return 18;
    if ((2 != 3) != 1) return 19;

    /* A constant condition folds the whole `?:`, including the arm with a
     * side effect that must never run. */
    int ran = 0;
    if ((1 ? 7 : (ran = 1)) != 7) return 20;
    if ((0 ? (ran = 1) : 9) != 9) return 21;
    if (ran != 0) return 22;

    /* Division by zero is NOT folded (fold_const declines), so the trap stays
     * a trap instead of silently becoming a number. This only checks that the
     * compiler still compiles it. */
    int zero = 0;
    int guarded = (zero != 0) ? 100 / zero : 5;
    if (guarded != 5) return 23;

    /* Short-circuit still short-circuits with a constant on the left. */
    if (0 && (ran = 1)) return 24;
    if (ran != 0) return 25;

    /* Folding must not disturb lvalues or pointer arithmetic. */
    int a[4];
    a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4;
    a[1 + 1] = 42;
    if (a[2] != 42) return 26;
    int *p = a;
    if (*(p + 1) != 2) return 27;
    if (p + 0 != p) return 28;      /* pointer result: must stay a pointer */

    /* `char` arithmetic folds in the 64-bit domain, like the runtime does. */
    char c = 'A';
    if (c + 1 != 'B') return 29;

    return 0;
}
