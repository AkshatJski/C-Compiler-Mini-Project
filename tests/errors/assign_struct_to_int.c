// EXPECT-ERROR: assignment to 'int' from incompatible type 'struct Point'
// Assigning a struct to an int used to copy 8 bytes of it silently (BUG-019).
struct Point {
    int x;
    int y;
};

int main(void) {
    int i;
    struct Point pt;
    pt.x = 1;
    pt.y = 2;
    i = pt;
    return i;
}
