// EXPECT-ERROR: conflicting type for parameter 1 of 'f'
int f(int x);

int f(char *x) {
    return 1;
}

int main(void) {
    return f(0);
}
