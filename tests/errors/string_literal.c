// EXPECT-ERROR: string literals are not supported
// A string literal must be skipped whole (not truncate the token stream) and
// reported as one clear "not supported" message (BUG-013).
int echo(char *s);

int main(void) {
    char *msg = "hello, world";
    return 0;
}
