// EXPECT: 65
// A string literal may legally contain NUL bytes, and every byte after the
// first NUL must still be emitted. Regression test for BUG-023: the decoded
// text was copied with strdup()/measured with strlen(), so "\0A" became "" and
// s[1] read whatever happened to follow in .rodata (65 is 'A').
int main(void) {
    char *s = "\0A";
    return s[1];
}
