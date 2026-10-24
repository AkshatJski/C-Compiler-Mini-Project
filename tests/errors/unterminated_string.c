// EXPECT-ERROR: unterminated string literal
// String literals are supported since ROADMAP Phase 4; the lexer's remaining
// string diagnostic is the one CLOSED on the same line. Before Phase 4 this
// test asserted the old "not supported" rejection (BUG-013).
int main(void) {
    char *msg = "never closed
    return 0;
}
