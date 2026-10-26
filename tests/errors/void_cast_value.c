// EXPECT-ERROR: void value used where a value is required
int main(void) {
    int x = (void)1;
    return x;
}
