/* 09_types_globals.c
 * char type (byte store + sign-extending load), global variables with
 * constant-folded initializers, void functions, and the implicit return 0
 * when control falls off the end of a function.
 * EXPECT: 0
 */
int g1 = 5;
int g2 = 2 * 3 + 4;      /* folded to 10 */
int g3 = 1 << 4;         /* folded to 16 */
int g4 = -7;
int g5 = 0xFF & 0x0F;    /* folded to 15 */
char gc = 'A';
int ga, gb = 3, gcc = 4; /* multiple declarators, mixed initialization */

int helper(void) {
    int unused = 0;
}   /* falls off the end: must implicitly return 0 */

void bump(void) { g1 = g1 + 1; }

void noret(void) { return; }

int main(void) {
    char c;

    if (g1 != 5) return 1;
    bump();
    bump();
    if (g1 != 7) return 2;

    if (g2 != 10) return 3;
    if (g3 != 16) return 4;
    if (g4 != -7) return 5;
    if (g5 != 15) return 6;
    if (gc != 65) return 7;

    if (ga != 0) return 8;
    if (gb != 3) return 9;
    if (gcc != 4) return 10;

    /* functions that fall off the end return 0 */
    if (helper() != 0) return 11;
    noret();

    /* char locals: assignment truncates to one byte */
    c = 'A';
    if (c != 65) return 12;
    c = 300;                 /* low byte is 44 */
    if (c != 44) return 13;
    c = c + 1;
    if (c != 45) return 14;

    /* signed char sign-extends on load */
    c = 200;
    if (c >= 0) return 15;
    if (c != -56) return 16;

    /* char arithmetic promotes to int */
    if (c + 100 != 44) return 17;

    return 0;
}
