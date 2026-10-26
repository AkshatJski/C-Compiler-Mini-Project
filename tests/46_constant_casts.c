/* 46_constant_casts.c
 * Integer casts are legal in C constant expressions. They also turn values
 * such as `(char)256` into the null pointer constant when the converted value
 * is zero, and they are valid case labels and global initializers.
 * EXPECT: 0
 */
int g = (int)3;
int h = (char)300; /* 44 after the implementation's signed-char conversion */

int main(void) {
    int c = 1;
    int *p = 0;
    int *q = c ? (int)0 : p;
    int *r = c ? p : (char)0;

    if (q != 0) return 1;
    if (r != 0) return 2;

    switch (2) {
        case (int)2:
            g += 1;
            break;
        default:
            return 3;
    }
    if (g != 4) return 4;
    if (h != 44) return 5;

    char ch = (char)300;
    if (ch != 44) return 6;

    unsigned char uc = (unsigned char)300;
    if (uc != 44) return 7;

    /* The converted value is 0, so this is a null pointer constant. */
    int *s = (char)256;
    if (s != 0) return 8;

    return 0;
}
