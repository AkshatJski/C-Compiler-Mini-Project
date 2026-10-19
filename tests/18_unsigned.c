/* 18_unsigned.c
 * `unsigned int` and `unsigned char`: unsigned division/modulo/right shift,
 * unsigned ordered comparison, and the zero-extending byte load -- while
 * signed char and C's integer promotion keep behaving exactly as before.
 * EXPECT: 0
 */

unsigned g_u;          /* bare `unsigned` == unsigned int, zero-filled */
unsigned char g_uc;

struct Rec { unsigned mask; unsigned char tag; };

int divu(unsigned a, unsigned b) { return a / b; }

int main(void) {
    /* A value with the top bit set: the discriminating case, because it is
     * negative if read as signed. Built by shifting, since literals that
     * large are out of range. */
    unsigned u = 1;
    u = u << 63;                    /* 0x8000000000000000 */

    /* Unsigned division: signed would give -2^62. */
    if (u / 2 != 4611686018427387904) return 1;

    /* Unsigned modulo: 2^63 % 3 == 2 (signed truncates toward 0 -> -2). */
    if (u % 3 != 2) return 2;

    /* Unsigned ordered comparison: 2^63 > 0 (signed would say otherwise). */
    if (!(u > 0)) return 3;
    if (u < 0) return 4;

    /* Logical shift, not arithmetic: sar would leave the top bits set. */
    if ((u >> 63) != 1) return 5;
    if ((u >> 4) != 576460752303423488) return 6;   /* 2^63 >> 4 == 2^59 */

    /* Unsigned survives a round trip through a function. */
    unsigned two = 2;
    if (divu(u, two) != 4611686018427387904) return 7;

    /* --- unsigned char: loaded with zero extension --- */
    unsigned char uc = 200;
    if (uc != 200) return 8;        /* a sign-extending load would give -56 */

    /* --- signed char still sign-extends --- */
    char sc = 200;
    if (sc != -56) return 9;

    /* --- C integer promotion: unsigned char operates as signed int --- */
    unsigned char a = 200;
    unsigned char b = 100;
    if (a + b != 300) return 10;
    if (a - b != 100) return 11;
    if (b - a != -100) return 12;   /* would wrap to a huge value if unsigned */

    /* --- mixed `unsigned int` / int: the unsigned operand wins --- */
    unsigned up1 = u + 1;           /* 0x8000000000000001 */
    if (up1 / 2 != 4611686018427387904) return 13;  /* signed gives -2^62+1 */

    /* --- globals --- */
    if (g_u != 0) return 14;
    if (g_uc != 0) return 15;
    g_u = u;
    g_uc = 7;
    if (g_u != u) return 16;
    if (g_uc != 7) return 17;

    /* --- unsigned members inside a struct --- */
    struct Rec r;
    r.mask = u;
    r.tag = 255;
    if (r.mask != u) return 18;
    if (r.tag != 255) return 19;    /* sign-extending load would give -1 */

    /* --- unsigned char array: zero-extending indexed load --- */
    unsigned char buf[4];
    buf[0] = 255; buf[1] = 1; buf[2] = 0; buf[3] = 128;
    if (buf[0] != 255) return 20;
    if (buf[3] != 128) return 21;
    if (buf[1] + buf[0] != 256) return 22;

    return 0;
}
