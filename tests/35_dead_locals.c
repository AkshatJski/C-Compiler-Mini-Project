// 35_dead_locals.c -- exercises the dead-store pass (ROADMAP Phase 5).
//
// Two locals are written and never read, so their stores must disappear under
// the optimizer and stay under --no-opt. Everything observable must survive:
// the returned value, a call whose *result* is discarded (the call still runs),
// and a variable whose address is taken (a pointer can read it).
//
// EXPECT: 12
// EXPECT-OUT: side-effect

int main(void) {
    int never_read = 5;
    int also_never_read = 0;
    int kept = 7;
    int address_taken = 3;
    int *p = &address_taken;

    also_never_read = kept * 2; /* pure right side, value unread: store goes */

    /* Impure right side: the store is dead but the call is not, so this whole
     * statement must survive. */
    never_read = printf("side-effect");

    *p = 5;
    return kept + address_taken; /* 7 + 5 = 12 */
}
