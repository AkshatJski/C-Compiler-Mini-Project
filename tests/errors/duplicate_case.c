// EXPECT-ERROR: duplicate case value 1
int main(void) {
    switch (1) {
        case 1: return 1;
        case 1: return 2;
    }
    return 0;
}
