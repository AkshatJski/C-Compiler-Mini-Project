// EXPECT-ERROR: used where a value is required
void f(void) { }

int main(void) {
    return f() + 1;
}
