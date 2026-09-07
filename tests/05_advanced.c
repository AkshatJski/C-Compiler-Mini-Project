/* 05_advanced.c
 * Operators and statements: %, &&, ||, !, for loops, compound assignment,
 * ++/-- (prefix and postfix), multiple declarators, block scoping.
 * EXPECT: 0
 */
int main(void) {
    int a, b, c = 10;
    int s = 0;
    int i;

    /* modulo */
    if (17 % 5 != 2) return 1;
    if (-17 % 5 != -2) return 2;

    /* compound assignment */
    a = 10;
    a += 5;
    if (a != 15) return 3;
    a -= 3;
    if (a != 12) return 4;
    a *= 2;
    if (a != 24) return 5;
    a /= 4;
    if (a != 6) return 6;
    a %= 4;
    if (a != 2) return 7;

    /* logical operators */
    if (!(1 && 1)) return 8;
    if (0 || 0) return 9;
    if (!0 != 1) return 10;
    if (1 && 0) return 11;
    if (0 && 1) return 12;

    /* short circuit: right operand must not run when left decides */
    a = 0;
    b = 0;
    if (a != 0 && (b = 1)) { } else { }
    if (b != 0) return 13;
    if (a == 0 || (b = 2)) { } else { }
    if (b != 0) return 14;
    if (1 || (1 / 0)) { } else { }

    /* for loop: basic */
    s = 0;
    for (i = 1; i <= 10; i = i + 1) {
        s = s + i;
    }
    if (s != 55) return 15;

    /* for loop: postfix ++ step and compound += body */
    s = 0;
    for (i = 1; i <= 10; i++) {
        s += i;
    }
    if (s != 55) return 16;

    /* for loop: single statement body */
    s = 0;
    for (i = 0; i < 5; i = i + 1) s = s + 2;
    if (s != 10) return 17;

    /* for loop: declaration in init, -- step */
    s = 0;
    for (int j = 5; j > 0; j--) {
        s = s + j;
    }
    if (s != 15) return 18;

    /* prefix ++ / -- */
    a = 5;
    b = ++a;
    if (a != 6 || b != 6) return 19;
    b = --a;
    if (a != 5 || b != 5) return 20;

    /* postfix ++ / -- */
    a = 5;
    b = a++;
    if (a != 6 || b != 5) return 21;
    b = a--;
    if (a != 5 || b != 6) return 22;

    /* multiple declarators with mixed initialization */
    a = 1;
    b = 2;
    c = 3;
    if (a != 1 || b != 2 || c != 3) return 23;

    /* block scoping / shadowing */
    c = 1;
    {
        int c = 2;
        if (c != 2) return 24;
    }
    if (c != 1) return 25;

    /* operator precedence with the new operators */
    if ((2 + 3) * 4 % 7 != 6) return 26;
    if (10 / 3 * 3 != 9) return 27;
    if (1 < 2 && 3 > 2) { } else { return 28; }
    if (1 > 2 || 2 > 1) { } else { return 29; }

    return 0;
}
