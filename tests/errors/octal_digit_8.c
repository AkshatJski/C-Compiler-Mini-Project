// EXPECT-ERROR: invalid digit '8' in octal constant
// "08" used to parse as 0, silently changing the program's meaning (BUG-008).
int main(void) {
    return 08;
}
