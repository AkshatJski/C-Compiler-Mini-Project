/* 16_member_incdec.c
 * Prefix and postfix ++ / -- applied to struct members accessed with both
 * `.` and `->`, including a char member. Regression test for the lvalue
 * check that rejected member operands.
 * EXPECT: 0
 */
struct Box {
    int v;
    char c;
};

int main(void) {
    struct Box b;
    struct Box *p;
    p = &b;

    b.v = 1;
    b.v++;              /* 2 */
    if (b.v != 2) return 1;
    ++b.v;              /* 3 */
    if (b.v != 3) return 2;
    b.v--;              /* 2 */
    if (b.v != 2) return 3;
    --b.v;              /* 1 */
    if (b.v != 1) return 4;

    p->v = 5;
    p->v++;             /* 6 */
    if (p->v != 6) return 5;
    ++p->v;             /* 7 */
    if (b.v != 7) return 6;

    b.c = 'A';
    b.c++;              /* 'B' == 66 */
    if (b.c != 66) return 7;
    ++b.c;              /* 'C' == 67 */
    if (b.c != 67) return 8;

    return 0;
}
