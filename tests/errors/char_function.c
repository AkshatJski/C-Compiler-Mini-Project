// EXPECT-ERROR: return type 'char' is not supported
char f(void) {
    return 1;
}

int main(void) {
    return f();
}
