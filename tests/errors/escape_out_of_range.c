// EXPECT-ERROR: hex escape value is too large for a byte
int main(void) {
    return '\x100';
}
