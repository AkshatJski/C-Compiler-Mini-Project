/* 49_adjacent_strings.c
 * Adjacent string literals are concatenated in translation phase 6, after
 * escape processing. The second literal here starts with a NUL byte, so a
 * text-based concatenation (strlen/strcat) would lose it.
 * EXPECT: 0
 */
int main(void) {
    char *s = "ab" "cd";
    if (s[0] != 'a' || s[2] != 'c' || s[4] != 0) return 1;

    char *t = "\0" "A";
    if (t[0] != 0 || t[1] != 'A' || t[2] != 0) return 2;

    char *empty = "" "";
    if (empty[0] != 0) return 3;

    return 0;
}
