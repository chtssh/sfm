#include <termbox.h>
#include <errno.h>
#include <fcntl.h> /* open */

#include "ui.h"
#include "nav.h"
#include "util.h"

static void ui_resize(struct ui *, int, int);
static void win_resize(struct window *, int, int, int, int);
static void win_draw(struct window *, struct colorscheme *);
static void win_printline(struct window *, int, int, struct color *, const char *);

struct ui *
ui_create(unsigned int *win_ratios,
	  struct dir **dir,
	  unsigned int num_wins,
	  struct colorscheme *scheme)
{
	int fd;
	struct ui *ui;
	unsigned int i;
	int width;
	int height;
	struct color *clr_dest, *clr_src;


	fd = open("/dev/tty", O_RDWR);

	switch (tb_init_fd(fd)) {
	case TB_EUNSUPPORTED_TERMINAL:
		perror("Unsupported terminal");
		exit(EXIT_FAILURE);
	case TB_EFAILED_TO_OPEN_TTY:
		perror("Failed to open TTY");
		exit(EXIT_FAILURE);
	case TB_EPIPE_TRAP_ERROR:
		perror("Pipe trap error");
		exit(EXIT_FAILURE);
	}

	ui = sfm_malloc(sizeof(struct ui));
	ui->num_wins = num_wins;
	ui->win = sfm_malloc(sizeof(struct window) * ui->num_wins);
	ui->sum_ratios = 0;
	ui->fd = fd;

	for (i = 0; i < ui->num_wins; ++i) {
		ui->sum_ratios += win_ratios[i];
		ui->win[i].ratio = win_ratios[i];
		ui->win[i].dir = dir + i;
	}

	clr_dest = (struct color *)&ui->clrscheme;
	clr_src = (struct color *)scheme;

	for (i = 0; i < sizeof(struct colorscheme)/sizeof(struct color); ++i) {
		clr_dest[i].fg = clr_src[i].fg + 1;
		clr_dest[i].bg = clr_src[i].bg + 1;
	}

	width = tb_width();
	height = tb_height();
	ui_resize(ui, width, height);

	return ui;
}

void
ui_free(struct ui *ui)
{
	tb_shutdown();
	free(ui->win);
	free(ui);
}

void
ui_redraw(struct ui *ui)
{
	unsigned int i;

	tb_clear();
	for (i = 0; i < ui->num_wins; ++i) {
		win_draw(ui->win + i, &ui->clrscheme);
	}
	tb_present();
}

void
ui_resize(struct ui *ui, int w, int h)
{
	unsigned int i;
	unsigned int acc;
	unsigned int width;

	for (acc = 0, i = 0; i < ui->num_wins - 1; ++i, acc += width) {
		width = (float)(ui->win[i].ratio) / ui->sum_ratios * w;
		win_resize(ui->win + i, acc, 0, width - 1, h - 1);
	}
	win_resize(ui->win + i, acc, 0, w - acc - 1, h - 1);
}

unsigned int *
ui_getch(struct ui *ui)
{
       struct tb_event event;
       static unsigned int key;
       (void)ui;

       if (tb_peek_event(&event, 0)) {
               key = event.ch ^ event.key;
               return &key;
       }

       return NULL;
}


void
win_resize(struct window *win, int x, int y, int w, int h)
{
	win->xmin = x;
	win->xmax = x + w;
	win->y = y;
	win->h = h;
}

void
win_draw(struct window *win, struct colorscheme *scheme)
{
	size_t i;
	int y;
	struct dir *dir = *(win->dir);
	const char *err_msg;
	struct color clr;

	if (dir == NULL)
		return;

	if (DIR_IS_FAIL(dir)) {
		switch (DIR_ERROR(dir)) {
		case EACCES:
			err_msg = "Permission denied.";
			break;
		case ENOENT:
			err_msg = "Directory doesn't exist.";
			break;
		default:
			err_msg = "Unknown error.";
			break;
		}
		win_printline(win, 0, 0, &scheme->err, err_msg);
		return;
	}

	if (DIR_IS_EMPTY(dir)) {
		win_printline(win, 0, 0, &scheme->empty, "Empty.");
		return;
	}

	for (i = dir->cf - dir->cl, y = 0; y <= win->h && i < dir->size; ++y, ++i) {
#define curmode (dir->fi[i]->st.st_mode)
		if (dir->fi[i]->realpath == dir->fi[i]->name)
			clr = scheme->lnkne;
		else if (dir->fi[i]->realpath != NULL)
			clr = scheme->lnk;
		else if (S_ISREG(curmode))
			if (curmode & 0111)
				clr = scheme->exe;
			else
				clr = scheme->reg;
		else if (S_ISDIR(curmode))
			clr = scheme->dir;
		else if (S_ISCHR(curmode))
			clr = scheme->chr;
		else if (S_ISBLK(curmode))
			clr = scheme->blk;
		else if (S_ISFIFO(curmode))
			clr = scheme->fifo;
		else if (S_ISSOCK(curmode))
			clr = scheme->sock;
		else
			clr = scheme->reg;

		if (i == dir->cf)
			clr.fg |= TB_REVERSE;


		win_printline(win, 1, y, &clr, dir->fi[i]->name);
	}

}

void
win_printline(struct window *win, int x, int y, struct color *clr, const char *str)
{
	uint32_t uch;

	for (y += win->y, x += win->xmin; x < win->xmax && *str; ++x) {
		str += tb_utf8_char_to_unicode(&uch, str);
		tb_change_cell(x, y, uch, clr->fg, clr->bg);
	}
	if (*str)
		tb_change_cell(win->xmax, y, '~', clr->fg, clr->bg);
}
