// EXPECT-ERROR: cannot dereference a 'void *' pointer
int main(void) {
    void *p;
    return *p;
}
