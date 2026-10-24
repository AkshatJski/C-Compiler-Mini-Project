// EXPECT-ERROR: assignment to 'int *' from incompatible type 'int'
// An integer is not an address. Assigning one to a pointer used to compile
// cleanly and then dereference a bogus address (BUG-019). The null constant 0
// is still allowed, and a cast is the explicit opt-in.
int main(void) {
    int i;
    int *p;
    i = 1;
    p = i;
    return *p;
}
