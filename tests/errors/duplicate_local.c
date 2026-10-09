// EXPECT-ERROR: duplicate declaration of 'a'
int main(void) {
    int a;
    int a;
    return 0;
}
