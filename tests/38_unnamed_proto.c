/* 38_unnamed_proto.c
 * C permits omitted parameter names in a prototype: `int f(int);`.
 * The declaration must still type-check calls against the parameter types.
 * EXPECT: 0
 */
int sum3(int, int, int);
int first(int *);

int main(void) {
    int values[3];
    values[0] = 9;
    values[1] = 8;
    values[2] = 7;

    if (sum3(1, 2, 3) != 6) return 1;
    if (first(values) != 9) return 2;
    return 0;
}

int sum3(int a, int b, int c) {
    return a + b + c;
}

int first(int *p) {
    return p[0];
}
