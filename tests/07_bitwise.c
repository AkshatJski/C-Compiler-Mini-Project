/* 07_bitwise.c
 * Bitwise and shift operators: & | ^ ~ << >>, their precedence, and the
 * compound assignment forms &= |= ^= <<= >>=.
 * EXPECT: 0
 */
int main(void) {
    int x;

    /* basic bitwise operators */
    if ((6 & 3) != 2) return 1;
    if ((6 | 1) != 7) return 2;
    if ((6 ^ 3) != 5) return 3;
    if ((~0) != -1) return 4;
    if ((~5) != -6) return 5;

    /* shifts (>> is arithmetic on signed values) */
    if ((1 << 4) != 16) return 6;
    if ((256 >> 4) != 16) return 7;
    if ((-8 >> 1) != -4) return 8;

    /* precedence: additive binds tighter than shift */
    if ((2 + 3 << 1) != 10) return 9;
    if ((1 << 2 + 3) != 32) return 10;

    /* precedence: shift binds tighter than &, then ^, then | */
    if ((1 << 2 & 7) != 4) return 11;
    if ((4 | 1 ^ 5 & 3) != 4) return 12;

    /* precedence: equality binds tighter than & */
    if ((7 & 3 == 3) != 1) return 13;
    if ((0 == 1 | 1) != 1) return 14;

    /* ~ desugared, works on any expression */
    if ((~(-1)) != 0) return 15;

    /* compound assignment forms */
    x = 0x0F;
    x &= 0x03;
    if (x != 3) return 16;
    x |= 0x30;
    if (x != 51) return 17;
    x ^= 0x01;
    if (x != 50) return 18;
    x <<= 1;
    if (x != 100) return 19;
    x >>= 1;
    if (x != 50) return 20;
    x <<= 2;
    if (x != 200) return 21;
    x >>= 3;
    if (x != 25) return 22;

    /* bitwise ops combine with arithmetic */
    if ((0xF0 & 0x0F) + (0xF0 | 0x0F) + (0xF0 ^ 0xFF) != 270) {
        return 23;
    }

    return 0;
}
