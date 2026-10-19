// EXPECT-ERROR: function return type 'struct' is not supported
struct S { int x; };

struct S make(void) {
    struct S s;
    s.x = 1;
    return s;
}

int main(void) {
    return 0;
}
