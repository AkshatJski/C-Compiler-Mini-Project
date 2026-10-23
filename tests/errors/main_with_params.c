// EXPECT-ERROR: 'main' must take no parameters
// mycc reads parameters from the SysV registers only, so on Windows a
// parameterised main would read whatever happened to be in %rdi (BUG-014).
int main(int argc, char **argv) {
    return argc;
}
