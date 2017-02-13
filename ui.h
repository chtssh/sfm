#ifndef UI_H
#define UI_H

#include <termbox.h>

struct window {
	int xmin;
	int xmax;
	int y;
	int h;
};

void win_resize(struct window *win, int x, int y, int w, int h);
void win_clear(struct window *win);
void wprint(struct window *win, int x, int y, int fg, int bg, const char *str);

#endif /* UI_H */
