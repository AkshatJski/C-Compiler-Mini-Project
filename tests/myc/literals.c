/* literals.c -- the C twin of tests/myc/literals.myc (BUG-029).
 *
 * Written to lower to the same AST as the myc file: myc's `if` bodies are
 * blocks, so the C uses braces; myc's `say` is print_str + print_endl.
 */

int main(void)
{
    if (0x10 != 16) {
        return 1;
    }

    if (010 != 8) {
        return 2;
    }

    if (0xFF != 255) {
        return 3;
    }

    if (0777 != 511) {
        return 4;
    }

    if (42u != 42) {
        return 5;
    }

    if (0 != 0) {
        return 6;
    }

    print_str("literals-ok");
    print_endl();
    return 42;
}
