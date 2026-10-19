/* 15_nested_switch.c
 * case / default labels nested inside a bare block and inside an if-body
 * within a switch, plus a nested switch whose labels belong to the inner
 * switch. Regression test for the dropped-label bug where only top-level
 * case labels were collected, folded and emitted.
 * EXPECT: 0
 */
int main(void) {
    int r;

    /* Labels nested in a bare block. */
    r = 0;
    switch (2) {
        {
            case 1:
                r = 1;
                break;
            case 2:
                r = 2;
                break;
        }
        default:
            r = 100;
    }
    if (r != 2) return 1;

    /* Case label inside an `if` body; control then falls through to
     * `default`. */
    r = 0;
    switch (5) {
        if (1)
            case 5:
                r = 5;
        default:
            r += 50;
    }
    if (r != 55) return 2;

    /* Nested switch owns its own labels and must not disturb the outer
     * numbering. */
    r = 0;
    switch (1) {
        case 1:
            switch (2) {
                case 2:
                    r = 7;
                    break;
                default:
                    r = 9;
            }
            break;
        default:
            r = 11;
    }
    if (r != 7) return 3;

    return 0;
}
