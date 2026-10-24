/* 21_global_pointers.c
 * Pointer declarators at file scope. Regression test for BUG-005: the
 * top-level parser demanded an identifier straight after the base type, so
 * `int *gp;` and `struct Node *head;` were rejected with
 * "expected name after type".
 * EXPECT: 0
 */

struct Node {
    int value;
    struct Node *next;
};

int store[8];
char *gcp;
int *gp;
int **gpp;
struct Node *ghead;

int *pick(int i);

int main(void) {
    int local = 42;
    int *lp = &local;

    /* a global pointer can be assigned and dereferenced */
    gp = lp;
    if (*gp != 42) return 1;

    *gp = 7;
    if (local != 7) return 2;

    /* ... and pointed at, and pointed at twice */
    gpp = &gp;
    if (**gpp != 7) return 3;

    /* struct pointers work at file scope too */
    struct Node n1;
    n1.value = 11;
    n1.next = 0;
    ghead = &n1;
    if (ghead->value != 11) return 4;
    if (ghead->next != 0) return 5;

    /* a pointer-returning prototype defined later in the file */
    *pick(5) = 99;
    if (store[5] != 99) return 6;

    /* char pointer globals store a full address */
    gcp = (char *)&local;
    if (*(int *)gcp != 7) return 7;

    /* the per-declarator star still means what C says it means:
     * `int a, *b;` declares one int and one pointer */
    if (gp == 0) return 8;
    return 0;
}

int *pick(int i) {
    return &store[i];
}
