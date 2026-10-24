/* hello.c
 * The smallest mycc program with visible output (ROADMAP Phase 4).
 *
 * print_str / print_int are compiler built-ins: semantic analysis lowers them
 * to printf / putchar calls, so no #include and no declaration are needed.
 * The call still crosses the real platform ABI into the C runtime, which is
 * what finally lets a mycc program print instead of only returning a number.
 *
 *   mycc hello.c -o hello.s
 *   gcc -o hello hello.s
 *   ./hello
 *       Hello, world!
 *       120
 *       3 + 4 = 7, and 2 * 21 = 42
 */
int fact(int n) {
    if (n < 2) return 1;
    return n * fact(n - 1);
}

int main(void) {
    print_str("Hello, world!\n");

    print_int(fact(5));                 /* 120, decimal + newline */

    /* --dump-ast shows print_int above as printf("%lld\n", ...) and this as a
     * plain variadic printf: 3 arguments in registers, no special cases. */
    printf("3 + 4 = %lld, and 2 * 21 = %lld\n", 3 + 4, 2 * 21);
    return 0;
}
