/* 22_struct_assign.c
 * Struct-to-struct assignment copies the whole object. Regression test for
 * BUG-003: codegen used to store a single 8-byte word, so every struct with
 * more than one member was silently truncated.
 * EXPECT: 0
 */

struct Point {
    int x;
    int y;
};

/* Five ints: 40 bytes, so a single 8-byte store cannot possibly be enough. */
struct Big {
    int a;
    int b;
    int c;
    int d;
    int e;
};

int main(void) {
    struct Point p;
    struct Point q;

    p.x = 1;
    p.y = 2;
    q = p;
    if (q.x != 1) return 1;
    if (q.y != 2) return 2;

    /* the copies are independent */
    q.x = 9;
    if (p.x != 1) return 3;

    /* chained assignment: the value of `q = p` is the object itself */
    struct Point r;
    r = q = p;
    if (r.x != 1 || r.y != 2) return 4;
    if (q.x != 1) return 6;

    /* larger than one register */
    struct Big m;
    struct Big n;
    m.a = 10;
    m.b = 20;
    m.c = 30;
    m.d = 40;
    m.e = 50;
    n = m;
    if (n.a != 10) return 7;
    if (n.b != 20) return 8;
    if (n.c != 30) return 9;
    if (n.d != 40) return 10;
    if (n.e != 50) return 11;

    n.c = 99;
    if (m.c != 30) return 12;

    /* assignment through a pointer */
    struct Point *pp = &p;
    *pp = q;
    if (p.x != 1 || p.y != 2) return 13;

    /* assignment through a struct pointer's arrow */
    pp->x = 5;
    p = *pp;
    if (p.x != 5) return 14;

    return 0;
}
