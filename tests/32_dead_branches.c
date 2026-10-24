/* 32_dead_branches.c
 * ROADMAP Phase 5: constant-condition elimination.
 * EXPECT: 0
 *
 * The dead arm is not emitted, but it is still parsed and type-checked -- so
 * this test also documents the rule that deleting code from the *output* must
 * never delete a *diagnostic*. The `tests/errors/` suite covers the rejection
 * half of that; here we prove the surviving half executes correctly.
 *
 * Every `if (0)` / `while (0)` below contains an assignment that would change
 * the result if the compiler emitted it anyway.
 */

int side_effects = 0;

int bump(void) {
    side_effects = side_effects + 1;
    return 1;
}

int main(void) {
    int x = 0;

    /* The classic: only the taken arm may run. */
    if (0) { x = 99; }
    if (1) { x = 7; }
    if (x != 7) return 1;

    /* else-branch selection. */
    if (0) { x = 99; } else { x = 8; }
    if (x != 8) return 2;
    if (1) { x = 9; } else { x = 99; }
    if (x != 9) return 3;

    /* Loops whose body never runs. */
    while (0) { x = 99; }
    if (x != 9) return 4;

    /* `for` keeps its init (real effects) and drops the body. */
    for (x = 0; 0; x = x + 1) { x = 99; }
    if (x != 0) return 5;

    /* `do { } while (0)` runs exactly once -- and its labels must still
     * exist, because a break/continue in the body needs a target. */
    do { x = x + 1; } while (0);
    if (x != 1) return 6;

    do { x = x + 1; if (x > 5) break; } while (0);
    if (x != 2) return 7;

    /* `continue` inside a do-while(0) still lands on the step. */
    do { continue; x = 99; } while (0);
    if (x != 2) return 8;

    /* A constant condition over a call: the call must not happen. */
    if (0 && bump()) { x = 99; }
    if (side_effects != 0) return 9;

    /* A folded condition, not just a literal. */
    if (2 + 2 == 5) { x = 99; }
    if (x != 2) return 10;
    if (2 + 2 == 4) { x = 11; }
    if (x != 11) return 11;

    /* A non-constant condition must still branch normally. */
    int y = 0;
    if (x == 11) { y = 1; } else { y = 2; }
    if (y != 1) return 12;

    /* `while (0)` wrapping a real loop must not disturb the outer state. */
    while (0) { while (1) { x = 99; } }
    if (x != 11) return 13;

    return 0;
}
