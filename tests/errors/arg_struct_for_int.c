// EXPECT-ERROR: passing 'struct Point' to parameter 'v' of 'takes_int'
// Argument types are now checked against the parameter types (BUG-019).
struct Point {
    int x;
    int y;
};

int takes_int(int v) {
    return v;
}

int main(void) {
    struct Point pt;
    pt.x = 1;
    pt.y = 2;
    return takes_int(pt);
}
