/* 41_void_expressions.c
 * A void expression is legal as a discarded expression statement and on the
 * left of the comma operator, including as a for-loop step. Its value must not
 * be used (see tests/errors/void_value.c).
 * EXPECT: 0
 */
void bump(int *p) {
    (*p)++;
}

int main(void) {
    int i = 0;
    int n = 0;

    for (; i < 3; bump(&n), i++) {
    }
    if (i != 3) return 1;
    if (n != 3) return 2;

    (void)n;
    (void)(n + 1);
    (void)(void)n;

    int j = 0;
    int m = 0;
    for (; j < 2; bump(&m)) {
        j++;
    }
    if (m != 2) return 3;

    int c = 1;
    c ? bump(&n) : bump(&n);
    if (n != 4) return 4;
    c ? bump(&n) : 0;
    if (n != 5) return 5;
    return 0;
}
