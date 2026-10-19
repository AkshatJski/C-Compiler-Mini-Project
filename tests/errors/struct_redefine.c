// EXPECT-ERROR: redefinition of 'struct S'
struct S { int x; };
struct S { int y; };

int main(void) {
    struct S s;
    s.x = 1;
    return s.x;
}
