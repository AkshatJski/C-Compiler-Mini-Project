/* 48_unsigned_arithmetic_types.c
 * Result types drive later signed/unsigned operations. Addition,
 * subtraction, comparisons, shifts and ?: must follow C's usual arithmetic
 * conversions instead of always taking the left operand's type.
 * EXPECT: 0
 */
int g1 = ((unsigned int)-1 > 0);
int g2 = (0 - (unsigned int)2) / 2;
int g3 = ((unsigned int)-1) / 2;

int main(void) {
    unsigned int m = (unsigned int)-1;
    unsigned int u = 2;
    unsigned int b = 0;
    unsigned int s = 1;
    int i = -2;
    int j = -2;

    b--; /* b is now UINT_MAX */

    /* A comparison has type int, even though its operands are unsigned. */
    if ((u < 3) / -1 != -1) return 1;

    /* int + unsigned int is unsigned. */
    if ((0 + b) / 2 != b / 2) return 2;

    /* int - unsigned int is unsigned too. */
    if ((1 - u) / 2 != m / 2) return 3;

    /* A shift result has the promoted left operand's type; the unsigned
     * count must not turn `i >> s` into a logical shift. */
    if ((i >> s) != -1) return 4;
    if ((i >> s) / 2 != 0) return 13; /* the shift result is signed int */
    j >>= s;
    if (j != -1) return 5;

    /* Both signed and unsigned conditional branches make the result unsigned. */
    if ((1 ? -1 : u) / 2 != m / 2) return 6;
    if (!((1 ? -1 : u) > 0)) return 7;
    if ((0 ? -1 : b) / 2 != b / 2) return 8;

    /* The same conversions apply while folding global initializers and case
     * labels, not just to code generated for function bodies. */
    if (g1 != 1) return 9;
    if (g2 != g3) return 10;
    int r = 0;
    switch (2) {
        case ((unsigned int)1 > 0) * 2:
            r = 1;
            break;
        default:
            return 11;
    }
    if (r != 1) return 12;

    return 0;
}
