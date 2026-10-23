/* 25_fall_off_end.c
 * Falling off the end of a non-void function is undefined in C, so it must be
 * warned about -- but only when it is genuinely reachable. Regression test for
 * BUG-016: the compiler used to return 0 with no diagnostic at all.
 * EXPECT: 0
 * WARN: control reaches end of non-void function 'maybe'
 */
int maybe(int a) {
    if (a) return 1;
}

/* every path returns: no warning */
int all_paths(int a) {
    if (a) return 1;
    else return 2;
}

/* an endless loop never reaches the closing brace: no warning */
int never_returns(void) {
    while (1) {
        if (1) break;
    }
    return 4;
}

/* a switch with a default whose last region returns: no warning */
int with_switch(int a) {
    switch (a) {
    case 1:
        return 1;
    default:
        return 0;
    }
}

/* a do/while that always returns in its body: no warning */
int do_returns(void) {
    do {
        return 3;
    } while (0);
}

int main(void) {
    if (maybe(0) != 0) return 1;
    if (all_paths(0) != 2) return 2;
    if (with_switch(1) != 1) return 3;
    if (with_switch(9) != 0) return 4;
    if (do_returns() != 3) return 5;
    if (never_returns() != 4) return 6;
    return 0;
}
