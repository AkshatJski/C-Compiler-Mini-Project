// EXPECT-ERROR: preprocessor directives are not supported
// A preprocessor directive is not a lexer error; saying so once, clearly, is
// better than "unexpected character '#'" (BUG-012).
#include <stdio.h>
int main(void) {
    return 0;
}
