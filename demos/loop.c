/* demos/loop.c
 * A for loop that adds the numbers 1..10.
 *   1 + 2 + ... + 10 = 55
 * The result is the program's exit code.
 */
int main(void) {
    int s;
    int i;
    s = 0;
    for (i = 1; i <= 10; i++) {
        s += i;
    }
    return s;
}
