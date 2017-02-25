#ifndef UI_H
#define UI_H

#include <termbox.h>

struct window {
	int xmin;
	int xmax;
	int y;
	int h;
};

void win_resize(struct window *, int, int, int, int);
void wprint(struct window *, int, int, int, int, const char *);

#endif /* UI_H */
