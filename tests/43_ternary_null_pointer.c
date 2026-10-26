/* 43_ternary_null_pointer.c
 * A conditional whose branches are a pointer and the null pointer constant
 * has pointer type, regardless of which branch is written first.
 * EXPECT: 0
 */
int main(void) {
    int value = 7;
    int *q = &value;
    int *p = q;
    int c = 1;

    p = c ? 0 : q;
    if (p != 0) return 1;

    c = 0;
    p = c ? 0 : q;
    if (p != q) return 2;

    c = 1;
    p = c ? q : 0;
    if (p != q) return 3;

    c = 0;
    if ((c ? 0 : q) != q) return 4;
    c = 1;
    if ((c ? 0 : q) != 0) return 5;

    return 0;
}
