/* demos/broken.c - intentionally wrong, to show error reporting.
 * Semantic analysis catches both mistakes and points at the exact
 * line and column of each one:
 *
 *     'nope' is not declared
 *     'add' expects 2 argument(s), got 3
 *
 * Run:  mycc demos/broken.c
 */
int add(int a, int b) {
    return a + b;
}
int main(void) {
    int x;
    x = nope;
    return add(1, 2, 3);
}
