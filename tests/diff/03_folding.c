/* Differential test 03 -- constant folding and constant conditions
 * (ROADMAP Phase 5).
 *
 * Cross-checked against the host compiler: both must exit 0. Folding is the
 * first pass in the project that rewrites the program before codegen, so the
 * only acceptable outcome is that the rewritten program behaves identically.
 * gcc is the oracle here precisely because it has its own, far more aggressive
 * folder -- if the two disagree, mycc folded something wrongly.
 *
 * Width-agnostic on purpose: mycc's `int` is 64-bit and gcc's is 32-bit, so
 * every check stays inside 32-bit range and none depends on overflow.
 */

int calls = 0;

int tick(void) {
    calls = calls + 1;
    return 1;
}

int main(void) {
    /* Precedence is preserved by folding. */
    if (2 + 3 * 4 != 14) return 1;
    if ((2 + 3) * 4 != 20) return 2;

    /* Nested folds. */
    if (1 + 2 + 3 + 4 + 5 + 6 != 21) return 3;
    if ((10 - 4) * (3 + 3) != 36) return 4;

    /* Division and remainder truncate toward zero in C. */
    if (7 / 2 != 3) return 5;
    if ((-7) / 2 != -3) return 6;
    if ((-7) % 2 != -1) return 7;

    /* Bit operations. */
    if ((1 << 8) != 256) return 8;
    if ((255 & 15) != 15) return 9;
    if ((240 | 15) != 255) return 10;
    if ((240 ^ 255) != 15) return 11;
    if ((~0) != -1) return 12;

    /* Logical results are 0/1, never the operand. */
    if ((5 && 7) != 1) return 13;
    if ((0 || 0) != 0) return 14;

    /* Comparisons. */
    if ((3 < 4) != 1) return 15;
    if ((4 <= 3) != 0) return 16;
    if ((4 > 4) != 0) return 17;
    if ((4 >= 4) != 1) return 18;

    /* Constant condition, dead arm has a side effect that must not happen. */
    if ((1 ? 5 : tick()) != 5) return 19;
    if (calls != 0) return 20;
    if ((0 ? tick() : 6) != 6) return 21;
    if (calls != 0) return 22;

    /* Constant conditions around statements. */
    int x = 0;
    if (0) { x = 90; }
    if (1) { x = x + 1; } else { x = 91; }
    while (0) { x = 92; }
    for (x = 1; 0; x = x + 1) { x = 93; }
    do { x = x + 1; } while (0);
    if (x != 2) return 23;

    /* Short-circuit with a constant left operand: no call, no fold. */
    if (0 && tick()) return 24;
    if (calls != 0) return 25;
    if (1 || tick()) { x = x + 1; }
    if (calls != 0) return 26;
    if (x != 3) return 27;

    /* Folded index arithmetic still addresses the right element. */
    int a[5];
    int i;
    for (i = 0; i < 5; i = i + 1) { a[i] = i * i; }
    if (a[2 + 1] != 9) return 28;
    if (a[3 * 1] != 9) return 29;

    /* A non-constant condition is untouched and still branches. */
    int y = 4;
    if (y == 4) { x = x + 1; } else { x = 99; }
    if (x != 4) return 30;

    return 0;
}
