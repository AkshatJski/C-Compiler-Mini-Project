/* 4_pair.c -- the same program as demo\4_pair.myc, written in C.
 *
 * Both files produce the same AST, so both produce the same assembly. This one
 * is here to be compared against, not to be read: it exists so the claim
 * "myc is a real front end, not a translator" can be checked with `fc /b`
 * instead of believed.
 */

int twice(int n) {
    return n * 2;
}

int main(void) {
    int total = 0;
    int i = 1;
    char *name = "myc";

    while (i <= 5) {
        total = total + twice(i);
        i = i + 1;
    }

    print_str("hello from ");
    print_endl();
    print_str(name);
    print_endl();
    print_int(total);

    if (total == 30 && 0 == 0) {
        print_str("correct");
        print_endl();
    }

    return total;
}
