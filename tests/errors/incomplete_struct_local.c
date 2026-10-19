// EXPECT-ERROR: isn't known
struct U;

int main(void) {
    struct U v;
    v = 5;
    return 0;
}
