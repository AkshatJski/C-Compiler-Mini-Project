/* 06_control_flow.c
 * do-while, switch/case/default (with fallthrough), break and continue in
 * loops and switch statements, including nesting.
 * EXPECT: 0
 */
int classify(int x) {
    switch (x) {
        case 0: return 100;
        case 1:
        case 2: return 200;   /* fallthrough from case 1 */
        default: return 300;
    }
}

int main(void) {
    int i;
    int s;
    int n;
    int count;

    /* do-while runs its body at least once */
    n = 0;
    count = 0;
    do { count++; n++; } while (n < 5);
    if (count != 5) return 1;

    /* do-while executes once even when the condition is false */
    count = 0;
    do { count++; } while (0);
    if (count != 1) return 2;

    /* continue inside do-while jumps to the condition test only */
    i = 0;
    s = 0;
    do {
        i++;
        if (i == 3) continue;
        s += i;
    } while (i < 5);
    if (s != 12) return 3;   /* 1 + 2 + 4 + 5 */

    /* switch fallthrough accumulation up to an explicit break */
    s = 0;
    switch (2) {
        case 1: s += 1;
        case 2: s += 2;
        case 3: s += 4;
                break;
        case 4: s += 8;
    }
    if (s != 6) return 4;

    /* default label matches */
    s = 0;
    switch (9) {
        case 1: s = 1; break;
        default: s = 42; break;
    }
    if (s != 42) return 5;

    /* no matching case and no default: nothing runs */
    s = 7;
    switch (99) { case 1: s = 1; break; }
    if (s != 7) return 6;

    /* stacked case labels share one body */
    if (classify(0) != 100) return 7;
    if (classify(1) != 200) return 8;
    if (classify(2) != 200) return 9;
    if (classify(7) != 300) return 10;

    /* break inside a switch nested in a loop breaks only the switch */
    s = 0;
    for (i = 0; i < 10; i++) {
        switch (i) {
            case 3: break;       /* leaves the switch, loop keeps going */
            default: s += i;
        }
    }
    if (s != 42) return 11;      /* 0..9 except 3 = 45 - 3 */

    /* continue inside a switch nested in a loop continues the loop */
    s = 0;
    i = 0;
    do {
        i++;
        switch (i) {
            case 2: continue;    /* skips the trailing s += 100 */
            default: s += i;
        }
        s += 100;
    } while (i < 4);
    if (s != 308) return 12;     /* i=1: 101; i=2: continue; i=3: 204; i=4: 308 */

    return 0;
}
