/* 33_constant_operands.c
 * ROADMAP Phase 5: a literal right operand is encoded inside the instruction
 * instead of being given a register (addq $1, %rax rather than
 * push/load/pop/add).
 * EXPECT: 0
 *
 * This pass changes *how* an operation is emitted, not what it means, so it is
 * exactly the kind of change that can be silently wrong: the immediate form
 * uses a different set of x86 instructions (3-operand imulq, 8-bit shift
 * counts, sign-extended 32-bit immediates, unsigned setcc with an immediate
 * compare). Each of those has its own trap, which is what this test collects.
 *
 * The division and modulo cases deliberately stay on the general path (a
 * constant divisor still has to trap on zero); they are here to prove the
 * fallback is used and still correct.
 */

int main(void) {
    int a[6];
    int i;
    for (i = 0; i < 6; i = i + 1) { a[i] = i * 3; }
    if (a[5] != 15) return 1;

    int x = 100;

    /* add/sub immediates */
    if (x - 40 != 60) return 2;
    if (x + -8 != 92) return 3;

    /* imulq $imm, %rax, %rax -- the three-operand form */
    if (x * 3 != 300) return 4;
    if (x * -1 != -100) return 5;

    /* div/mod keep the general path and must still be right */
    if (x / 3 != 33) return 6;
    if (x % 7 != 2) return 7;

    /* bitwise immediates */
    if ((x & 12) != 4) return 8;
    if ((x | 3) != 103) return 9;
    if ((x ^ 96) != 4) return 10;

    /* shift by an 8-bit immediate, both signedness flavours */
    if ((x << 3) != 800) return 11;
    if ((x >> 2) != 25) return 12;
    unsigned int u = 8;
    if ((u >> 2) != 2) return 13;

    /* comparisons: same flags, so the setcc must still pick the right one */
    if (!(x >= 100)) return 14;
    if (!(x <= 100)) return 15;
    if (x == 99) return 16;
    if (x != 100) return 17;
    if (!(x > 99)) return 18;
    if (!(x < 101)) return 19;

    /* A pointer scaled by its element size: `p + 1` becomes an immediate. */
    int *p = a;
    if (*(p + 1) != 3) return 20;
    if (p[5] != 15) return 21;

    /* Negative operands: truncating division still rounds toward zero. */
    int neg = -50;
    if (neg / 7 != -7) return 22;
    if (neg % 7 != -1) return 23;

    /* char values are 64-bit here, so `c + 1` is a plain immediate add. */
    char c = 65;
    if (c + 1 != 66) return 24;
    if ((c == 65) != 1) return 25;

    /* An immediate that does not fit in a signed 32-bit field must fall back
     * to the general path rather than emit a truncated instruction. */
    int big = 5;
    if (big + 3000000000 != 3000000005) return 26;
    if (big - 3000000000 != -2999999995) return 27;

    return 0;
}
