// EXPECT: 0
// EXPECT-OUT: 42
// The four built-in printing helpers: print_int (decimal, newline),
// print_char (one character), print_endl (a newline) and print_str (a string).
// They need no declaration and no #include.
int main(void) {
    print_int(40 + 2);
    print_char('!');
    print_endl();
    print_str("built-ins ok\n");
    return 0;
}
