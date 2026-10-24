/* 36_registers.c
 * Stress test for the expression temporary pool (ROADMAP Phase 5): values kept
 * in %rbx/%r12 instead of being pushed, spilled back to the stack when the pool
 * runs out, and parked around calls.
 *
 * Every case below is one the pool can get wrong in a way that still compiles:
 *
 *  1. right-nested expressions deeper than the pool (2), so entries 3+ spill
 *  2. division, modulo and shifts nested inside those expressions -- those
 *     instructions use %r10/%r11 as scratch and %cl for the count
 *  3. calls evaluated while temporaries are live (the pool must be parked)
 *  4. calls at the deepest point of an expression that has already spilled
 *  5. compound assignment with a call on the right, through an array element
 *  6. postfix `i++` as an index together with a call
 *  7. a call result stored through a pointer while another value is live
 *  8. nested call arguments (allocation pressure on the argument path)
 *  9. a call inside a shift count, with the left operand live across it
 *
 * EXPECT: 42
 */

int f(int n) { return n * 2; }

int main(void) {
    int a = 3;
    int b = 4;
    int c = 5;
    int d = 6;
    int sum = 0;

    /* 1: deeper than the pool */
    if (1 + (2 + (3 + (4 + (5 + (6 + 7))))) != 28) return 1;

    /* 2: div/mod and shifts inside nested expressions */
    if ((a / b) + (d % c) != 1) return 2;
    if ((a << 2) + (d >> 1) != 15) return 3;

    /* 3: two calls, one of them multiplied */
    if (f(1) + f(2) * f(3) != 26) return 4;

    /* 4: a call at the bottom of an expression that already spilled */
    if (a + (b + (c + (d + f(5)))) != 28) return 5;

    /* 5: compound assignment with a call on the right */
    int arr[4];
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;
    int i = 1;
    arr[i] += f(10);
    if (arr[1] != 22) return 6;

    sum += f(3);
    if (sum != 6) return 7;

    /* 6: postfix as an index, and a call on the right */
    i = 0;
    arr[i++] += f(1);
    if (arr[0] != 3 || i != 1) return 8;

    /* 7: store through a pointer while a temporary is live. `a` becomes 8. */
    int *p = &a;
    if ((*p = f(4)) + a != 16) return 9;

    /* 8: nested call arguments */
    if (f(f(f(1))) != 8) return 10;

    /* 9: call in the shift count, left operand (8) live across it */
    if ((a << f(1)) != 32) return 11;

    /* 10: unsigned shifts and comparisons at depth */
    unsigned int u = 1;
    if ((u << 3) + (u >> 1) != 8) return 12;

    /* 11: division and modulo inside an expression whose left operand is already
     * parked -- the shape that first exposed the %r10 scratch conflict */
    if (((a * b) % c) + ((c * d) / 2) != 17) return 13;

    /* 12: two nested divisions, neither of which may lose the outer value */
    if ((100 / (5 / 5)) + (9 % (4 % 3)) != 100) return 14;

    /* 13: a call inside the divisor of a live left operand */
    if ((a * 4) / f(2) != 8) return 15;

    /* 14: a call under a modulo */
    if ((f(5) % c) != 0) return 16;

    /* 15: right-nested, with a division at the bottom and a shift above it */
    if (a + (b * (c + (d / 2))) != 40) return 17;
    if ((c << (b / 2)) - d != 14) return 18;

    return 42;
}
