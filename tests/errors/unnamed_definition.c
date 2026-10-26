// EXPECT-ERROR: parameter name is required in a function definition
int f(int) {
    return 0;
}

int main(void) {
    return f(1);
}
