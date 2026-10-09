// EXPECT-ERROR: multiple 'default' labels in one switch
int main(void) {
    switch (1) {
        default: return 0;
        default: return 1;
    }
    return 0;
}
