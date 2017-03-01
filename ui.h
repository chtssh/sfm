#ifndef UI_H
#define UI_H

#include <termbox.h>

struct window {
	int xmin;
	int xmax;
	int y;
	int h;
};

struct clrscheme {
	uint16_t fg;
	uint16_t bg;
};

void win_resize(struct window *, int, int, int, int);
void wprint(struct window *, int, int, struct clrscheme *, const char *);

#endif /* UI_H */
