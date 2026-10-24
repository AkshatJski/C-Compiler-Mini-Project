// EXPECT-ERROR: function 'helper' is declared but never defined
// A called-but-undefined function must be reported by the compiler with the
// call site's coordinates, not by the linker against a temporary .s (BUG-015).
int helper(int x);

int main(void) {
    return helper(3);
}
