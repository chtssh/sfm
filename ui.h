#ifndef SFM_UI_H
#define SFM_UI_H

#include <stddef.h>

struct window {
	int xmin;
	int xmax;
	int y;
	int h;
	unsigned int ratio;
};

struct color {
	unsigned int fg;
	unsigned int bg;
};

struct colorscheme {
	struct color reg, dir, lnk, exe, chr, blk, fifo, sock, lnkne, empty, err;
};

struct ui {
	struct nav *nav;
	struct colorscheme clrscheme;
	struct window *win;
	unsigned int num_wins;
	unsigned int sum_ratios;
	int fd;
};

enum { COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
	COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
};

enum { ATTR_BOLD = 0x0100 };

struct ui * ui_create(struct nav *,
		      const unsigned int *,
		      unsigned int,
		      struct colorscheme *);
void ui_free(struct ui *);
void ui_redraw(struct ui *);
unsigned int * ui_getch(struct ui *);
void ui_on_resize(struct ui *);
void ui_line_up(struct ui *, size_t);
void ui_line_down(struct ui *, size_t);

#endif /* SFM_UI_H */
