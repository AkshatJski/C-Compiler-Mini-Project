// EXPECT-ERROR: parameter has incomplete type
struct U;

int f(struct U v) {
    return 0;
}

int main(void) {
    return 0;
}
