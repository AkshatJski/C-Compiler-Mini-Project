/* 03_loops.c
 * while loops, accumulators, and early return from a loop.
 * EXPECT: 0
 */
int main(void) {
    int i;
    int sum;

    /* Sum 0..9 = 45 */
    i = 0;
    sum = 0;
    while (i < 10) {
        sum = sum + i;
        i = i + 1;
    }
    if (sum != 45) return 1;

    /* Factorial 5 = 120, counting down */
    i = 5;
    sum = 1;
    while (i > 0) {
        sum = sum * i;
        i = i - 1;
    }
    if (sum != 120) return 2;

    /* Loop with complex body and relational condition */
    i = 0;
    while (i * i <= 100) {
        i = i + 1;
    }
    if (i != 11) return 3;

    /* Conditional inside a loop, infinite loop with early return */
    i = 0;
    while (1) {
        i = i + 1;
        if (i == 5) {
            return 0;
        }
    }
    return 9; /* unreachable */
}
