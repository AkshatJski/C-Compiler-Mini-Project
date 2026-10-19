// EXPECT-ERROR: function return type 'char' is not supported
//
// `unsigned char` is still a one-byte return type, and this subset only
// allows `int`/`void` function returns -- so it must be rejected the same
// way plain `char` is.
unsigned char bad(void) {
    return 1;
}

int main(void) {
    return 0;
}
