/* 19_comma_operator.c
 * The comma operator: `a, b` evaluates `a` for its side effects and yields
 * `b`. Regression test for BUG-007 -- before the fix `(a = 1, a + 1)` was
 * rejected and `f(1, 2)` could not be parsed at all.
 * EXPECT: 0
 */
int sum(int a, int b) {
    return a + b;
}

int main(void) {
    int a;
    int b;

    /* sequencing: the value comes from the right operand */
    a = (b = 3, b + 1);
    if (a != 4) return 1;
    if (b != 3) return 2;

    if ((1, 2) != 2) return 3;

    /* the left operand runs even when its value is thrown away */
    int i = 0;
    int x = (i++, i++, 7);
    if (x != 7) return 4;
    if (i != 2) return 5;

    /* a comma in an argument list must stay a separator */
    if (sum(1, 2) != 3) return 6;
    if (sum((1, 2), (3, 4)) != 6) return 7;

    /* ... and in a declarator initializer, where it starts the next name */
    int p = 5, q = 6;
    if (p != 5 || q != 6) return 8;

    /* comma in a for-header */
    int s = 0;
    for (i = 0, b = 10; i < 3; i++, b--) {
        s = s + 1;
    }
    if (s != 3) return 9;
    if (b != 7) return 10;

    /* comma in a return statement */
    return (1, 0);
}
