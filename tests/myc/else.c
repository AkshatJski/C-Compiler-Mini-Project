/* else.c -- the C twin of tests/myc/else.myc (BUG-027).
 *
 * Written to lower to the *same* AST as the myc file: myc's `if` bodies are
 * blocks, so the C uses braces; myc's `say` is print_str + print_endl or
 * print_int, so those are spelled out. Byte-identical assembly is the check.
 */

int pick(int n)
{
    if (n > 5) {
        return 1;
    } else {
        return 2;
    }
}

int main(void)
{
    int n = 3;

    if (pick(n) == 2) {
        print_str("else-branch");
        print_endl();
    } else {
        print_str("if-branch");
        print_endl();
    }

    if (n > 5) {
        print_int(5);
    } else {
        print_int(8);
    }

    return 8;
}
