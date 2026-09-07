/* example.c
 * End-to-end demonstration of the compiler feature set:
 * functions, recursion, parameters, for loops, if/else, arithmetic,
 * modulo %, logical &&/||/!, compound assignment, ++/--, globals,
 * local variables, block scoping.
 *
 *   fact(5)            = 120
 *   sum_primes_up_to(20) = 2+3+5+7+11+13+17+19 = 77
 *   total              = 120 + 77 = 197
 *
 * The result (197) is returned by main as the process exit code,
 * trivially visible via $?.
 * EXPECT: 197
 */
int g_calls;

int is_prime(int n) {
    if (n < 2) return 0;
    for (int d = 2; d <= n / 2; d = d + 1) {
        if (n % d == 0) return 0;
    }
    return 1;
}

int sum_primes_up_to(int n) {
    int s;
    s = 0;
    for (int i = 2; i <= n; i++) {
        if (is_prime(i)) {
            s += i;
        }
    }
    return s;
}

int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

int main(void) {
    int total;
    int calls;
    total = fact(5) + sum_primes_up_to(20);
    g_calls++;
    calls = g_calls;
    if (total == 197 && calls == 1 && is_prime(7) && !is_prime(1)) {
        return total;
    }
    return 255;
}
