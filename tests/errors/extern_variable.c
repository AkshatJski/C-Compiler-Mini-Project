// EXPECT-ERROR: 'extern' is only supported on function declarations
extern int shared;

int main(void) {
    return shared;
}
