/* 11_scope.c
 * Scoping rules: for-init declarations are loop-local, consecutive loops may
 * reuse the same name, and inner blocks shadow outer bindings.
 * EXPECT: 0
 */
int main(void) {
    int s;
    int i;

    /* the for-init declaration must not leak out of the first loop */
    s = 0;
    for (int i = 0; i < 3; i++) { s += i; }
    for (int i = 0; i < 3; i++) { s += i; }
    if (s != 6) return 1;

    /* a for-declared name hides an outer variable only inside the loop */
    i = 100;
    for (int i = 0; i < 2; i++) { s += 0; }
    if (i != 100) return 2;

    /* while/do-while share the surrounding scope */
    s = 0;
    i = 0;
    while (i < 3) { int inner = i; s += inner; i++; }
    if (s != 3) return 3;

    /* variables declared in a block disappear with the block */
    s = 0;
    {
        int a = 10;
        s += a;
        {
            int a = 20;
            s += a;
        }
        s += a;
    }
    if (s != 40) return 4;

    /* for-body block also gets its own scope */
    s = 0;
    for (i = 0; i < 3; i++) {
        int b = i * 2;
        s += b;
    }
    if (s != 6) return 5;

    return 0;
}
