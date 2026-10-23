// EXPECT: 0
// EXPECT-OUT: Hello, world!
// ROADMAP Phase 4: the first mycc program whose output you can actually see.
// print_str is a built-in that lowers to printf("%s", ...) through the
// platform calling convention.
int main(void) {
    print_str("Hello, world!\n");
    return 0;
}
