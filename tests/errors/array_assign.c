// EXPECT-ERROR: cannot assign to an array
int main(void) {
    int a[2];
    a[0] = 1;
    a = 5;
    return a[0];
}
