/* 24_bom.c
 * A UTF-8 byte-order mark at the start of a source file is legal and must be
 * skipped instead of being lexed as two stray characters. Regression test for
 * BUG-011: editors on Windows write this BOM by default.
 * EXPECT: 42
 */
int main(void) {
    return 42;
}