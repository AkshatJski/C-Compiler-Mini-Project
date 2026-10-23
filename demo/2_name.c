/* 2_name.c -- the interactive moment of the demo.
 *
 * The whole compiler is exercised here and none of it is a trick: a local
 * char array, an array-to-pointer decay at the call, an extern function
 * resolved through the platform ABI, three string literals in .rodata, and
 * printf writing the answer. Nothing about this program knows it is being
 * demonstrated.
 *
 * Two details matter for a live demo:
 *  - the prompt ends with '\n' so stdout is flushed before scanf() blocks.
 *    Without it the audience stares at a blank screen until they type, and
 *    an interactive demo looks broken at exactly the wrong moment;
 *  - scanf() is declared with one parameter but called with two. That is
 *    legal because extern prototypes use variadic arity (argc >=
 *    param_count) -- which is precisely why printf needs no declaration at
 *    all and scanf needs only this one line.
 *
 *   mycc 2_name.c -o 2_name.s
 *   gcc -o 2_name.exe 2_name.s
 *   2_name.exe
 */
extern int scanf(char *fmt);

int main(void) {
    char name[64];

    print_str("What is your name?\n");

    /* %63s: the width stops a long name from overrunning the buffer. The
     * array decays to char *, which is what scanf expects. */
    scanf("%63s", name);

    print_str("Hello, ");
    print_str(name);
    print_str("!  I just compiled that myself.\n");

    return 0;
}
