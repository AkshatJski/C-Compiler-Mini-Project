/* 10_ternary.c
 * The conditional (ternary) operator: results, right-associativity, nesting,
 * and its precedence relative to other operators.
 * EXPECT: 0
 */
int main(void) {
    int x;
    int y;

    if ((1 ? 2 : 3) != 2) return 1;
    if ((0 ? 2 : 3) != 3) return 2;

    /* right-associative: a ? b : (c ? d : e) */
    if ((1 ? 4 : 0 ? 5 : 6) != 4) return 3;
    if ((0 ? 4 : 1 ? 5 : 6) != 5) return 4;
    if ((0 ? 4 : 0 ? 5 : 6) != 6) return 5;

    /* only the selected branch is evaluated */
    x = 5;
    if ((x > 3 ? x * 2 : x / 2) != 10) return 6;
    if ((x < 3 ? x * 2 : x / 2) != 2) return 7;

    /* conditional binds looser than ||, &&, comparisons */
    if ((0 || 1 ? 7 : 8) != 7) return 8;
    if ((1 && 1 ? 9 : 10) != 9) return 9;
    if ((x == 5 ? 1 : 0) != 1) return 10;

    /* usable as an assignment source, including negative results */
    y = x > 0 ? -1 : -2;
    if (y != -1) return 11;
    y = x < 0 ? -1 : -2;
    if (y != -2) return 12;

    /* nested inside a condition */
    if ((x == 5 ? 1 : 0) && (x != 6 ? 1 : 0)) {
        /* expected path */
    } else {
        return 13;
    }

    return 0;
}
