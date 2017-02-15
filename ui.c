#include "ui.h"

void
win_resize(struct window *win, int x, int y, int w, int h)
{
	win->xmin = x;
	win->xmax = x + w;
	win->y = y;
	win->h = h;
}

void
wprint(struct window *win, int x, int y, int fg, int bg, const char *str)
{
	uint32_t uch;

	for (y += win->y, x += win->xmin; x < win->xmax && *str; ++x) {
		str += tb_utf8_char_to_unicode(&uch, str);
		tb_change_cell(x, y, uch, fg, bg);
	}
	if (*str)
		tb_change_cell(win->xmax, y, '~', fg, bg);
}
