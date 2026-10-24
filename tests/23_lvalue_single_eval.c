/* 23_lvalue_single_eval.c
 * An lvalue with a side effect must be evaluated exactly once by `op=` and by
 * postfix `++`/`--`. Regression test for BUG-001 and BUG-002: `a[i++] += 5`
 * advanced `i` twice (and, with `a[i++]++`, stored through the wrong address).
 * EXPECT: 0
 */

int store[4];
int calls = 0;

int *slot(int i) {
    calls = calls + 1;
    return &store[i];
}

int main(void) {
    int i = 0;
    int a[4];

    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    a[3] = 40;

    /* compound assignment through a subscript with a side effect */
    a[i++] += 5;
    if (i != 1) return 1;
    if (a[0] != 15) return 2;

    /* postfix ++ through a subscript with a side effect */
    int j = 0;
    int old = a[j++]++;
    if (j != 1) return 3;
    if (old != 15) return 4;
    if (a[0] != 16) return 5;

    /* the same through a pointer returned by a call: one call, not two */
    store[0] = 1;
    store[1] = 2;
    int k = 0;
    *slot(k++) += 5;
    if (k != 1) return 6;
    if (calls != 1) return 7;
    if (store[0] != 6) return 8;

    /* postfix through the same shape */
    calls = 0;
    k = 0;
    int prev = (*slot(k++))++;
    if (k != 1) return 9;
    if (calls != 1) return 10;
    if (prev != 6) return 11;
    if (store[0] != 7) return 12;

    /* an lvalue without side effects still behaves */
    a[2] += 2;
    if (a[2] != 32) return 13;
    a[3]--;
    if (a[3] != 39) return 14;
    a[3] -= 1;
    if (a[3] != 38) return 15;

    /* the plain-variable fast path must not regress either */
    int v = 4;
    v += 6;
    if (v != 10) return 16;
    v *= 2;
    if (v != 20) return 17;
    int w = v++;
    if (w != 20 || v != 21) return 18;

    /* compound assignment yields the new value */
    int z = 0;
    z = a[0] += 4;
    if (z != 20) return 19;
    if (a[0] != 20) return 20;

    return 0;
}
