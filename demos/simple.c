/* demos/simple.c
 * The smallest meaningful demo: a function, a multiplication,
 * a function call, an addition, and a return.
 *   square(5) + 1 = 26
 * The result is the program's exit code.
 */
int square(int x) {
    return x * x;
}
int main(void) {
    return square(5) + 1;
}
