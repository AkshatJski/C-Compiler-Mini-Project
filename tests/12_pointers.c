/* 12_pointers.c
 * Pointer declaration, address-of &, dereference *, mutation via pointer,
 * pointers across function calls, and pointer arithmetic.
 * EXPECT: 0
 */

int add_ten(int *ptr) {
    *ptr = *ptr + 10;
    return *ptr;
}

int main(void) {
    int x;
    int *p;
    int **pp;

    x = 42;
    p = &x;

    /* Basic dereference read */
    if (*p != 42) return 1;

    /* Mutation via pointer */
    *p = 100;
    if (x != 100) return 2;

    /* Pointer to pointer */
    pp = &p;
    **pp = 200;
    if (x != 200) return 3;

    /* Function call mutating caller's stack frame */
    add_ten(&x);
    if (x != 210) return 4;

    /* Pointer arithmetic (scaling by 8 bytes for int*) */
    int a;
    int b;
    a = 10;
    b = 20;
    int *base = &b;
    /* In this stack layout, locals are placed at adjacent 8-byte slots */
    if (*(base) != 20) return 5;

    return 0;
}