/* 17_multidim_arrays.c
 * Multi-dimensional arrays: nested indexing, the row stride, row decay to
 * int*, pointer arithmetic across rows, 3-D arrays, and a global 2-D array.
 * EXPECT: 0
 */

int sum_row(int *row, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += row[i];
    return s;
}

int gm[2][3];

int main(void) {
    int m[3][4];
    int i, j;

    /* Fill and read back every element. */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            m[i][j] = i * 10 + j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            if (m[i][j] != i * 10 + j) return 1;

    /* Row stride must be 4 ints, not 3: m[1][0] is the 5th value written. */
    if (m[1][0] != 10) return 2;
    if (m[2][3] != 23) return 3;

    /* A row decays to int*. */
    int *row = m[1];
    if (row[0] != 10) return 4;
    if (row[3] != 13) return 5;
    if (sum_row(m[2], 4) != 86) return 6;   /* 20+21+22+23 */

    /* *(m + 1) is the address of row 1 (stride = 4 ints = 32 bytes). */
    int *row2 = *(m + 1);
    if (row2[1] != 11) return 7;

    /* Row difference is measured in elements of the row type (ints). */
    if (m[2] - m[1] != 4) return 8;

    /* Three dimensions. */
    int t[2][3][4];
    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++)
            for (int k = 0; k < 4; k++)
                t[i][j][k] = i * 100 + j * 10 + k;
    if (t[0][0][0] != 0) return 9;
    if (t[1][0][0] != 100) return 10;
    if (t[1][2][3] != 123) return 11;

    /* Global 2-D array is zero-filled, then writable. */
    if (gm[0][0] != 0) return 12;
    if (gm[1][2] != 0) return 13;
    gm[1][2] = 42;
    if (gm[1][2] != 42) return 14;

    /* Nested index on the left of an assignment. */
    m[0][0] = 999;
    if (m[0][0] != 999) return 15;

    return 0;
}
