/* 02_conditionals.c
 * if / else-if / else control flow and relational operators.
 * EXPECT: 0
 */
int main(void) {
    int x;
    int y;

    x = 10;
    y = 0;

    if (x > 5) {
        y = 1;
    } else {
        y = 2;
    }
    if (y != 1) return 1;

    if (x == 10) y = 3; else y = 4;
    if (y != 3) return 2;

    if (x < 5) return 3;
    if (x <= 10) y = 5; else y = 6;
    if (y != 5) return 4;

    if (x != 10) return 5;
    if (x >= 11) return 6;

    /* Nested if inside else chain (else-if) */
    if (x == 5) {
        y = 7;
    } else if (x == 10) {
        y = 8;
    } else {
        y = 9;
    }
    if (y != 8) return 7;

    /* if with no else, then fallthrough */
    y = 0;
    if (x == 10) y = 42;
    if (y != 42) return 8;

    /* Condition is any expression */
    if (x * 2 - 20) return 9;   /* x*2-20 == 0 is false, so no return */

    return 0;
}
