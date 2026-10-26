/* 42_assignment_values.c
 * The value of an assignment is the value stored after conversion to the
 * lvalue's type. A store to char therefore also narrows the value left in the
 * result register, for both simple and compound assignments.
 * EXPECT: 0
 */
int main(void) {
    char c = 0;
    int x = (c = 300);
    if (c != 44) return 1;
    if (x != 44) return 2;

    unsigned char u = 0;
    int y = (u = 300);
    if (u != 44) return 3;
    if (y != 44) return 4;

    c = 0;
    x = (c += 300);
    if (c != 44) return 5;
    if (x != 44) return 6;

    u = 0;
    y = (u -= 300);
    if (u != 212) return 7;
    if (y != 212) return 8;

    char *p = &c;
    c = 0;
    x = (*p = 300);
    if (c != 44) return 9;
    if (x != 44) return 10;

    c = 0;
    x = (*p += 300);
    if (c != 44) return 11;
    if (x != 44) return 12;

    if ((c = 256) != 0) return 13;
    if (c != 0) return 14;

    x = (c = 255);
    if (c != -1) return 15;
    if (x != -1) return 16;

    char d = 127;
    if (++d != -128) return 17;
    if (d != -128) return 18;
    if (--d != 127) return 19;
    if (d != 127) return 20;

    return 0;
}
