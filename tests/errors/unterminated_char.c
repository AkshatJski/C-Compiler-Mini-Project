// EXPECT-ERROR: unterminated character literal
int main(void) {
    char c = 'a;
    return c;
}
