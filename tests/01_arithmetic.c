/* 01_arithmetic.c
 * Arithmetic operators, literals, precedence, unary minus, comments.
 * EXPECT: 0
 */
int main(void) {
    int a;
    int b;
    int c;

    /* Assignment and binary arithmetic */
    a = 7;
    b = 3;
    c = a * b - (a + b) / 2;   /* 21 - 5 = 16 */
    if (c != 16) return 1;

    /* Precedence: * and / bind tighter than + and - */
    if (2 + 3 * 4 != 14) return 2;
    if (100 - 30 * 3 != 10) return 3;
    if ((2 + 3) * 4 != 20) return 4;

    /* Division truncates toward zero */
    if (a / b != 2) return 5;
    if (-a / b != -2) return 6;

    /* Unary minus */
    c = -a;
    if (c != -7) return 7;

    /* Integer literal expressions */
    if (123456 + 654321 != 777777) return 8;
    if (a * (b - 1) != 14) return 9;

    /* Block comments are ignored */
    /* this block comment spans
       multiple lines */
    if (a + b != 10) return 10;

    return 0;
}
