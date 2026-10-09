/* 08_literals.c
 * Integer literal bases (hex/octal) and suffixes, plus character constants
 * and escape sequences.
 * EXPECT: 0
 */
int main(void) {
    /* hexadecimal */
    if (0xFF != 255) return 1;
    if (0x1F != 31) return 2;
    if (0Xff != 255) return 3;
    if (0xDEAD != 57005) return 4;

    /* octal */
    if (010 != 8) return 5;
    if (0777 != 511) return 6;
    if (0 != 0) return 7;

    /* decimal baseline */
    if (100 != 100) return 8;

    /* integer suffixes are accepted and ignored */
    if (10u != 10) return 9;
    if (10L != 10) return 10;
    if (10UL != 10) return 11;
    if (10ll != 10) return 12;

    /* character literals */
    if ('A' != 65) return 13;
    if ('a' != 97) return 14;
    if ('0' != 48) return 15;

    /* escapes */
    if ('\n' != 10) return 16;
    if ('\t' != 9) return 17;
    if ('\r' != 13) return 18;
    if ('\0' != 0) return 19;
    if ('\\' != 92) return 20;
    if ('\'' != 39) return 21;

    /* hex escapes */
    if ('\x41' != 65) return 22;
    if ('\x7f' != 127) return 23;

    /* literal used directly in arithmetic */
    if ('z' - 'a' != 25) return 24;
    if (0x10 * 2 != 32) return 25;

    return 0;
}
