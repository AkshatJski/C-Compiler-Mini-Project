// EXPECT-ERROR: conflicting return type for 'f'
int f(int x);

char *f(int x) {
    return 0;
}

int main(void) {
    return f(1);
}
