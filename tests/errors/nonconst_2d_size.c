// EXPECT-ERROR: array size must be an integer constant
//
// A multi-dimensional array still requires constant bounds, and the bound
// check applies to every dimension, not just the first.
int main(void) {
    int n = 4;
    int m[2][n];
    return 0;
}
