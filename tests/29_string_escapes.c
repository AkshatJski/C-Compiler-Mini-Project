// EXPECT: 0
// EXPECT-OUT: ESCAPES-OK
// Every escape the lexer accepts, exercised through a string literal rather
// than only through a char literal. Regression test for BUG-022: octal escapes
// (\101) were rejected outright, which no test noticed while string literals
// were still unsupported -- the moment a literal became real data, normal C
// code such as "\101" failed to compile.
int main(void) {
    print_str("quote[\"] backslash[\\] tab[\t] percent[%%]\n");
    print_str("octal[\101] hex[\x41] bell-ok\n");
    print_str("ESCAPES-OK\n");
    return 0;
}
