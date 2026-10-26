/* A file-scope pointer can be initialized from 0 or another pointer, but a
 * nonzero integer is not an implicit pointer conversion. */
int *p = 1;

int main(void) {
    return 0;
}
