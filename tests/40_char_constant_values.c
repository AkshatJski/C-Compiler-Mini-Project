/* 40_char_constant_values.c
 * `char` is signed in this compiler, so a high-byte character constant has a
 * negative int value, exactly as GCC gives on a signed-char target. String
 * literals must still preserve the raw byte without sign extension.
 * EXPECT: 0
 */
int main(void) {
    if ('A' != 65) return 1;
    if ('\0' != 0) return 2;
    if ('\x7f' != 127) return 3;
    if ('\x80' != -128) return 4;
    if ('\200' != -128) return 5;
    if ('\xff' != -1) return 6;
    if ('\377' != -1) return 7;
    if ((unsigned char)'\xff' != 255) return 8;

    char *s = "\xff";
    if ((unsigned char)s[0] != 255) return 9;
    return 0;
}
