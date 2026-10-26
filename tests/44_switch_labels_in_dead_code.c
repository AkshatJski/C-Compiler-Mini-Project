/* 44_switch_labels_in_dead_code.c
 * Constant-condition elimination must not delete a case/default label that
 * an enclosing switch can jump to. The jump enters the dead-looking arm
 * directly, bypassing the if/loop condition.
 * EXPECT: 0
 */
int main(void) {
    int r = 0;

    switch (1) {
        if (0)
            case 1:
                r = 5;
        break;
        default:
            r = 7;
    }
    if (r != 5) return 1;

    r = 0;
    switch (2) {
        if (0) {
            case 2:
                r = 9;
                break;
        }
        default:
            r = 1;
    }
    if (r != 9) return 2;

    r = 0;
    switch (3) {
        while (0) {
            case 3:
                r = 11;
                break;
        }
        break;
        default:
            r = 12;
    }
    if (r != 11) return 3;

    r = 0;
    switch (4) {
        for (; 0;) {
            case 4:
                r = 13;
                break;
        }
        break;
        default:
            r = 14;
    }
    if (r != 13) return 4;

    r = 0;
    switch (5) {
        if (1)
            r = 0;
        else
            case 5:
                r = 15;
        break;
        default:
            r = 16;
    }
    if (r != 15) return 5;

    r = 0;
    switch (6) {
        if (0)
            default:
                r = 17;
        break;
        case 1:
            r = 18;
    }
    if (r != 17) return 6;

    return 0;
}
