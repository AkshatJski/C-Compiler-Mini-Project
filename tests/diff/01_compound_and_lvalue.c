/* 01_compound_and_lvalue.c — differential test: `op=` and postfix `++`/`--`
 * must evaluate an lvalue's address exactly once.
 *
 * This file is compiled twice by `difftest.bat` / `make difftest`: once by
 * mycc (then assembled) and once by the host gcc/clang. Both binaries must
 * return the same exit code (0 here). It exists because the exit-code suite
 * alone let BUG-001/002/003/004/020 through: every one of them returned a
 * plausible-looking number.
 *
 * WIDTH-AGNOSTIC ON PURPOSE: mycc's `int` is 64-bit while gcc's and clang's
 * are 32-bit, so this program never compares byte offsets, never uses
 * `sizeof`, and only compares pointers against `&a[i]` (identical in both
 * compilers) or arithmetic values that fit in 32 bits. `char` is one byte
 * everywhere.
 *
 * EXPECT: 0
 */

struct S { int v; char c; };

int a2[2];
int calls = 0;

int *slot(int i) {
    calls = calls + 1;
    return &a2[i];
}

int main(void) {
    int a[5];
    int n = 0;
    a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4; a[4] = 5;

    /* every compound operator, through a subscript */
    a[n] += 10;
    a[1] -= 1;
    a[2] *= 3;
    a[3] /= 2;
    a[4] %= 3;
    if (a[0] != 11 || a[1] != 1 || a[2] != 9 || a[3] != 2 || a[4] != 2) return 1;

    a[n] <<= 1;
    a[n] >>= 1;
    a[n] &= 3;
    a[n] |= 4;
    a[n] ^= 1;
    if (a[0] != 6) return 2;

    /* a char element: compound assignment truncates to one byte */
    char ca[2];
    ca[0] = 120;
    ca[0] += 10;
    if (ca[0] != -126) return 3;

    /* struct members */
    struct S s;
    s.v = 5;
    s.c = 1;
    s.v += 7;
    s.v *= 2;
    s.c += 2;
    if (s.v != 24) return 4;
    if (s.c != 3) return 5;

    /* pointer compound arithmetic scales by the element size */
    int *p = a;
    p += 2;
    if (p != &a[2]) return 6;
    p -= 1;
    if (p != &a[1]) return 7;

    /* char pointers too (char is one byte in both compilers) */
    char cb[8];
    char *cp = cb;
    cp += 3;
    if (cp != &cb[3]) return 8;

    /* pointer difference counts elements, not bytes */
    if (&a[4] - &a[0] != 4) return 9;

    /* the value of a compound assignment is the new value */
    int z = (a[2] += 1);
    if (z != 10 || a[2] != 10) return 10;

    /* the target reached through a call with a side-effecting index */
    int k = 0;
    a2[0] = 1; a2[1] = 2;
    *slot(k++) += 5;
    if (k != 1) return 11;
    if (calls != 1) return 12;
    if (a2[0] != 6) return 13;
    if (a2[1] != 2) return 14;

    /* ... and a postfix on the same shape */
    calls = 0;
    k = 0;
    int prev = (*slot(k++))++;
    if (k != 1) return 15;
    if (calls != 1) return 16;
    if (prev != 6 || a2[0] != 7) return 17;

    /* subscript with a side effect, verified end to end */
    int i = 0;
    int b[4];
    b[0] = 10; b[1] = 20; b[2] = 30; b[3] = 40;
    b[i++] += 5;
    if (i != 1) return 18;
    if (b[0] != 15) return 19;
    int j = 0;
    int old = b[j++]++;
    if (j != 1) return 20;
    if (old != 15 || b[0] != 16) return 21;
    if (b[1] != 20 || b[2] != 30 || b[3] != 40) return 22;

    /* member access through a side-effecting index */
    struct S *sp = &s;
    int m = 0;
    struct S *slots[2];
    slots[0] = &s;
    slots[1] = &s;
    slots[m++]->v += 1;
    if (m != 1) return 23;
    if (s.v != 25) return 24;

    /* a plain variable still uses the direct path */
    int v = 4;
    v += 6;
    v *= 2;
    if (v != 20) return 25;
    if (v++ != 20 || v != 21) return 26;
    if (++v != 22) return 27;

    return 0;
}
