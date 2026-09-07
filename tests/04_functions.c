/* 04_functions.c
 * Function declarations, parameters, calls, recursion, globals.
 * EXPECT: 0
 */
int g_count;

int add(int a, int b) {
    return a + b;
}

int sub(int a, int b) {
    return a - b;
}

int mul(int a, int b) {
    return a * b;
}

int divq(int a, int b) {
    return a / b;
}

int max(int a, int b) {
    if (a > b) return a;
    return b;
}

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int fact(int n) {
    int acc;
    acc = 1;
    while (n > 1) {
        acc = acc * n;
        n = n - 1;
    }
    return acc;
}

int main(void) {
    int a;
    int b;
    int c;

    a = 5;
    b = 3;

    c = add(a, b);
    if (c != 8) return 1;
    if (sub(a, b) != 2) return 2;
    if (mul(a, b) != 15) return 3;
    if (divq(a, b) != 1) return 4;
    if (max(a, b) != 5) return 5;

    if (fib(10) != 55) return 6;
    if (fact(5) != 120) return 7;

    /* Nested calls and calls inside expressions */
    if (add(add(a, b), add(1, 2)) != 11) return 8;
    if (mul(add(a, 1), sub(b, 1)) != 12) return 9;
    if (add(mul(a, b), divq(a, b)) != 16) return 10;

    /* Global variable read/write */
    g_count = 0;
    g_count = g_count + 1;
    if (g_count != 1) return 11;

    /* Forward reference: call a function defined later */
    if (later(9) != 81) return 12;

    return 0;
}

int later(int n) {
    return n * n;
}
