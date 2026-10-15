/* 13_arrays.c
 * 1D array declaration, indexing, writes, reads, loop traversal,
 * and array-to-pointer decay passed into functions.
 * EXPECT: 0
 */

int sum_array(int *arr, int len) {
    int total = 0;
    for (int i = 0; i < len; i++) {
        total += arr[i];
    }
    return total;
}

int main(void) {
    int a[5];

    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    a[3] = 40;
    a[4] = 50;

    /* Verify indexed reads */
    if (a[0] != 10) return 1;
    if (a[2] != 30) return 2;
    if (a[4] != 50) return 3;

    /* Verify pointer arithmetic equivalence: *(a + 3) == 40 */
    if (*(a + 3) != 40) return 4;

    /* Verify passing array to function (decay) */
    int total = sum_array(a, 5);
    if (total != 150) return 5;

    return 0;
}