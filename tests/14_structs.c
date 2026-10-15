/* 14_structs.c
 * Struct declaration, member access (.), the arrow operator (->),
 * struct pointers, and a forward function prototype.
 * EXPECT: 0
 */

struct Point {
    int x;
    int y;
};

/* Forward prototype declaration */
int get_x(struct Point *p);

int main(void) {
    struct Point pt;
    pt.x = 10;
    pt.y = 20;

    if (pt.x != 10) return 1;
    if (pt.y != 20) return 2;

    struct Point *p = &pt;

    /* Test arrow operator */
    p->x = 35;
    p->y = 45;
    if (pt.x != 35) return 3;
    if (pt.y != 45) return 4;

    /* Test function using struct pointer and prototype */
    if (get_x(&pt) != 35) return 5;

    return 0;
}

int get_x(struct Point *p) {
    return p->x;
}