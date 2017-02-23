#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "hash.h"
#include "ui.h"
#include "fs.h"

union arg {
	int i;
	unsigned u;
	float f;
	void *v;
};

struct key {
	char *codes;
	void (*func)(union arg *);
	union arg arg;
};

static void setup(void);
static void run(void);
static void cleanup(void) __attribute__((destructor));

static void move_v(union arg *);
static void move_h(union arg *);
static void move_page(union arg *);
static void goto_line(union arg *);
static void goto_dir(union arg *);
static void sort_files(union arg *);
static void quit(union arg *);

static int is_topdir_gt2col(void);
static int is_topdir_eq2col(void);

static int runner = 1;
static size_t prefix;

#define PREFIX_MULT(i)	(prefix > 0 ? prefix * (i) : (size_t)(i))

#include "config.h"

static struct window wins[LENGTH(column_ratios)];
static struct dir *dirs[LENGTH(column_ratios)];
#define dir_main	(dirs[LENGTH(dirs) - 2])
#define win_main	(wins[LENGTH(wins) - 2])
#define dir_preview	(dirs[LENGTH(dirs) - 1])
#define win_preview	(wins[LENGTH(wins) - 1])

static int (*is_topdir)(void) = LENGTH(dirs) > 2 ? is_topdir_gt2col : is_topdir_eq2col;
struct numratios { char check[LENGTH(column_ratios) < 2 ? -1 : 1]; };

void
dir_set_curline(int *cl, size_t cf)
{
	*cl = MIN(cf, (size_t)win_main.h);
}

int
is_topdir_gt2col(void)
{
	return dirs[LENGTH(dirs) - 3] == NULL;
}

int
is_topdir_eq2col(void)
{
	return dir_main->plen == 1;
}

void
on_resize(struct tb_event *ev)
{
	size_t i;
	unsigned acc, width;
	unsigned sum;

	for (sum = i = 0; i < LENGTH(column_ratios); ++i)
		sum += column_ratios[i];

	for (acc = 0, i = 0; i < LENGTH(column_ratios) - 1; ++i, acc += width) {
		width = (float)column_ratios[i] / sum * ev->w;
		win_resize(&wins[i], acc, 0, width - 1, ev->h - 1);
	}
	win_resize(&win_preview, acc, 0, ev->w - acc - 1, ev->h - 1);
}

void
wprintdir(struct window *win, struct dir *dir)
{
	size_t i;
	int y;

	if (dir == NULL)
		return;

	if (DIR_IS_FAIL(dir))
		switch (DIR_ERROR(dir)) {
		case EACCES:
			wprint(win, 0, 0, 0, TB_RED, "Permission denied.");
			return;
		case ENOENT:
			wprint(win, 0, 0, 0, TB_RED, "Directory doesn't exist.");
			return;
		default:
			wprint(win, 0, 0, 0, TB_RED, "Unknown error.");
			return;
		}

	if (DIR_IS_EMPTY(dir)) {
		wprint(win, 0, 0, 0, TB_DEFAULT, "Empty.");
		return;
	}

	for (i = dir->cf - dir->cl, y = 0; y <= win->h && i < dir->size; ++y, ++i)
		wprint(win, 1, y, 0, 0, dir->fi[i].name);
	wprint(win, 1, dir->cl, 0, TB_REVERSE, dir->fi[dir->cf].name);
}

void
screenredraw(void)
{
	size_t i;

	tb_clear();

	for (i = 0; i < LENGTH(wins); ++i)
		wprintdir(&wins[i], dirs[i]);

	tb_present();
}

void
move_v(union arg *arg)
{
	size_t scf = dir_main->cf;

	if (arg->i > 0)
		FILE_DOWN(dir_main, PREFIX_MULT(arg->i));
	else
		FILE_UP(dir_main, PREFIX_MULT(- arg->i));

	if (dir_main->cf != scf) {
		dir_main->cl += dir_main->cf - scf;
		if (dir_main->cl > win_main.h)
			dir_main->cl = win_main.h;
		else if (dir_main->cl < 0)
			dir_main->cl = 0;
		dir_preview = dir_child(dir_main);
		screenredraw();
	}
}

void
move_h(union arg *arg)
{
	struct dir *sd = dir_main;
	size_t i;

	if (arg->i > 0) {
		for (prefix = PREFIX_MULT(arg->i); dir_preview != NULL
		     && !DIR_IS_FAIL(dir_preview) && prefix > 0; --prefix) {
			for (i = 0; i < LENGTH(dirs) - 1; ++i)
				dirs[i] = dirs[i + 1];
			dir_preview = dir_child(dir_main);
		}
	} else {
		for (prefix = PREFIX_MULT(- arg->i); !is_topdir()
		     && prefix > 0; --prefix) {
			for (i = LENGTH(dirs) - 1; i > 0; --i)
				dirs[i] = dirs[i - 1];
			dirs[0] = dir_parent(dirs[1]);
		}
	}

	if (dir_main != sd)
		screenredraw();
}

void
move_page(union arg *arg)
{
	union arg a;

	a.i = (int)(win_main.h * arg->f);
	move_v(&a);
}

void
goto_dir(union arg *arg)
{
	struct dir *dir;
	size_t i;

	if ((dir = dir_get(arg->v)) == NULL)
		return;

	dir_main = dir;
	dir_preview  = dir_child(dir_main);
	for  (i = LENGTH(dirs) - 2; i-- != 0; )
		dirs[i] = dir_parent(dirs[i + 1]);

	screenredraw();
}

void
goto_line(union arg *arg)
{
	prefix += arg->u;
	if (prefix < dir_main->size)
		dir_main->cf = prefix;
	else
		dir_main->cf = dir_main->size - 1;
	dir_set_curline(&dir_main->cl, dir_main->cf);
	screenredraw();
}

void
sort_files(union arg *arg)
{
	size_t i;

	fs_set_sort(arg->u);
	for (i = 0; i < LENGTH(dirs); ++i)
		if (dirs[i] != NULL)
			dir_resort(dirs[i]);
	screenredraw();
}

void
quit(union arg *arg)
{
	(void)arg;
	runner = 0;
}

void
on_keypress(struct tb_event *ev)
{
	static struct key *pkeys[LENGTH(keys)];
	static int ci;
	struct key *ptmp[LENGTH(keys)];
	uint32_t code = ev->ch ^ ev->key;
	size_t i, j = 0;

	if (code >= '0' && code <= '9') {
		prefix = prefix * 10 + code - '0';
		return;
	}

	if (!ci) {
		for (i = 0; i < LENGTH(keys); ++i)
			if ((uint32_t)keys[i].codes[ci] == code)
				pkeys[j++] = &keys[i];
	} else {
		for (i = 0; pkeys[i] != NULL; ++i)
			if ((uint32_t)pkeys[i]->codes[ci] == code)
				ptmp[j++] = pkeys[i];
		for (i = 0; i != j; ++i)
			pkeys[i] = ptmp[i];
	}

	++ci;
	pkeys[j] = NULL;

	if (j == 1 && !(*pkeys)->codes[ci])
		goto callf;
	else if (j == 0)
		goto reset;

	/* TODO: print avalible keys */
	return;

callf:	(*pkeys)->func(&(*pkeys)->arg);
reset:	ci = prefix = 0;
}

void
setup(void)
{
	struct tb_event ev;
	union arg a;

	/* init ui */
	switch (tb_init()) {
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

	ev.w = tb_width();
	ev.h = tb_height();
	on_resize(&ev);

	/* init fs */
	fs_init();
	fs_set_sort(sort);

	a.v = getcwd(buffer, PATH_MAX);
	goto_dir(&a);
}

void
run(void)
{
	struct tb_event ev;

	while(runner && tb_poll_event(&ev) > 0)
		switch(ev.type) {
		case TB_EVENT_KEY:
			on_keypress(&ev);
			break;
		case TB_EVENT_RESIZE:
			on_resize(&ev);
			screenredraw();
			break;
		}
}

void
cleanup(void)
{
	fs_clean();
	tb_shutdown();
}

int
main(void)
{
	setup();
	run();
	return 0;
}
