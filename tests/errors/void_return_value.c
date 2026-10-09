// EXPECT-ERROR: should not return a value
void f(void) {
    return 1;
}

int main(void) {
    f();
    return 0;
}
