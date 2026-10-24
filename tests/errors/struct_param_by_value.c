// EXPECT-ERROR: which mycc does not support; pass a pointer instead
// Passing a struct by value needs an ABI mycc does not implement; accepting
// it silently copied 8 bytes of a larger object (BUG-004).
struct Point {
    int x;
    int y;
};

int sum_bad(struct Point p) {
    return p.x + p.y;
}

int main(void) {
    struct Point pt;
    pt.x = 1;
    pt.y = 2;
    return sum_bad(pt);
}
