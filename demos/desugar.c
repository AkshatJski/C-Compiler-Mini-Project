/* demos/desugar.c
 * Fancy syntax that the PARSER rewrites into a smaller core before
 * the rest of the compiler ever sees it. Run:
 *
 *     mycc --dump-ast demos/desugar.c
 *
 * and look at how these are shown as simpler forms:
 *
 *     a += 2     ->  a = a + 2
 *     b = a++    ->  keep old value, then a = a + 1
 *     if (!a)    ->  if (a == 0)
 *     return -b  ->  return 0 - b
 */
int main(void) {
    int a;
    int b;
    a = 1;
    a += 2;
    b = a++;
    if (!a) return -b;
    return a;
}
