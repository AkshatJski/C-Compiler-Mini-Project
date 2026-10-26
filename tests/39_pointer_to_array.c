/* 39_pointer_to_array.c
 * `&array` has pointer-to-array type, so `&array + 1` advances by the whole
 * array. Before the fix semantic analysis kept the decayed int* type, and
 * the step was scaled by one element instead.
 * EXPECT: 0
 */
int main(void) {
    int a[4];
    char b[3];

    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    a[3] = 4;

    int *end = (int *)(&a + 1);
    if (end != a + 4) return 1;
    if ((int *)(&a) != a) return 2;
    if (&a[0] + 1 != &a[1]) return 3;

    char *bend = (char *)(&b + 1);
    if (bend != b + 3) return 4;

    if ((&a + 1) - &a != 1) return 5;
    if ((&a + 2) - (&a + 1) != 1) return 6;
    return 0;
}
