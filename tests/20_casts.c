/* 20_casts.c
 * Explicit cast expressions `(type)expr`. Regression test for BUG-006: the
 * parser used to treat `(char)x` as a parenthesised expression and then choke
 * on the type keyword.
 * EXPECT: 0
 */
struct Wrap {
    int v;
};

int main(void) {
    int i = 300;

    /* narrowing to one byte keeps the low 8 bits */
    char c = (char)i;
    if (c != 44) return 1;
    unsigned char uc = (unsigned char)i;
    if (uc != 44) return 2;

    /* signed vs unsigned byte casts extend differently */
    char neg = (char)-1;
    if (neg != -1) return 3;
    unsigned char uneg = (unsigned char)-1;
    if (uneg != 255) return 4;

    /* pointer <-> integer needs a cast to be legal (BUG-019) */
    int *nullp = (int *)0;
    if (nullp != 0) return 5;

    int value = 7;
    int addr = (int)&value;
    int *back = (int *)addr;
    if (*back != 7) return 6;

    /* a cast binds tighter than any binary operator */
    if ((char)257 + 1 != 2) return 8;
    if ((char)1000 != -24) return 9;

    /* pointer casts keep the address; the result can be dereferenced */
    char *cp = (char *)&value;
    if (*((int *)cp) != 7) return 10;

    /* a cast is not an lvalue */
    struct Wrap w;
    w.v = 5;
    if (((struct Wrap *)&w)->v != 5) return 11;

    /* casts are fine in a condition */
    if (!(int *)&value) return 12;

    /* double cast through a pointer level */
    int **pp = &back;
    int ***ppp = (int ***)&pp;
    if (***ppp != 7) return 13;

    return 0;
}
