/* 47_question_escape.c
 * `\?` is a standard C escape sequence (6.4.4.4); it denotes '?'.
 * EXPECT: 0
 */
int main(void) {
    char c = '\?';
    char *s = "what\?";

    if (c != '?') return 1;
    if (s[0] != 'w' || s[4] != '?' || s[5] != '\0') return 2;
    return 0;
}
