// EXPECT: 0
// EXPECT-OUT: 7 14 21 28
// Calling a real library function through an `extern` declaration. mycc has no
// body for printf and no #include; the platform ABI (SysV or Windows x64) is
// what makes the call work. Four arguments here so that both register
// conventions are exercised without needing stack arguments.
extern int printf(char *fmt);

int main(void) {
    printf("%lld %lld %lld %lld\n", 7, 14, 21, 28);
    return 0;
}
