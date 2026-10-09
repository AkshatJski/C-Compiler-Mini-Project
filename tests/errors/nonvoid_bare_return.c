// EXPECT-ERROR: must return a value
int f(void) {
    return;
}

int main(void) {
    return f();
}
