/* 45_label_controlled_statement.c
 * A case/default label in the unbraced body of an if/loop labels the statement
 * after it. That statement belongs to the control construct: when the if/loop
 * is skipped on the normal path, it must be skipped too. Switch traversal must
 * still find every label and the code inside it.
 * EXPECT: 0
 */
int main(void) {
    int r = 0;

    switch (2) {
        case 2:
            if (0)
                case 1:
                    r = 5;
            r += 1;
            break;
        default:
            r = 9;
    }
    if (r != 1) return 1;

    r = 0;
    switch (2) {
        case 2:
            if (0)
                case 1:
                    r = 5;
            else
                r = 7;
            break;
        default:
            r = 9;
    }
    if (r != 7) return 2;

    r = 0;
    switch (2) {
        case 2:
            while (0)
                case 1:
                    r = 5;
            break;
        default:
            r = 9;
    }
    if (r != 0) return 3;

    r = 0;
    switch (2) {
        case 2:
            for (; 0;)
                case 1:
                    r = 5;
            break;
        default:
            r = 9;
    }
    if (r != 0) return 4;

    /* Consecutive labels stay nested, and both are jump targets. */
    r = 0;
    switch (1) {
        if (0)
            case 1:
                case 2:
                    r = 9;
        break;
        default:
            r = 10;
    }
    if (r != 9) return 5;

    r = 0;
    switch (2) {
        if (0)
            case 1:
                case 2:
                    r = 9;
        break;
        default:
            r = 10;
    }
    if (r != 9) return 6;

    r = 0;
    switch (3) {
        case 3:
            if (0)
                case 1:
                    case 2:
                        r = 9;
            break;
        default:
            r = 10;
    }
    if (r != 0) return 7;

    /* A read in a controlled label body keeps the store to x alive. */
    int x = 3;
    r = 0;
    switch (1) {
        if (0)
            case 1:
                r = x;
        break;
        default:
            r = 0;
    }
    if (r != 3) return 8;

    r = 0;
    switch (4) {
        case 4:
            do
                case 1:
                    r = 5;
            while (0);
            break;
        default:
            r = 0;
    }
    if (r != 5) return 9;

    /* The normal true path also keeps the labelled statement in the loop. */
    r = 0;
    int i = 0;
    switch (5) {
        case 5:
            while (i++ < 3)
                case 1:
                    r++;
            break;
        default:
            r = 0;
    }
    if (r != 3) return 10;

    r = 0;
    switch (6) {
        case 6:
            for (i = 0; i < 3; i++)
                case 1:
                    r += 2;
            break;
        default:
            r = 0;
    }
    if (r != 6) return 11;

    r = 0;
    switch (7) {
        case 7:
            if (1)
                case 1:
                    r = 8;
            break;
        default:
            r = 0;
    }
    if (r != 8) return 12;

    return 0;
}
