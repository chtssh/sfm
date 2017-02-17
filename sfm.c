#include <errno.h>
#include <string.h>

#include "util.h"
#include "hash.h"
#include "ui.h"
#include "fs.h"

enum { PosLeft, PosMid, PosRight, PosLast };

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

/* global variables */
static struct window wins[PosLast];
static struct dir *dirs[PosLast];
static int runner = 1;
static size_t prefix;

#define PREFIX_MULT(i)	(prefix > 0 ? prefix * (i) : (size_t)(i))

#include "config.h"

/* TODO: Remove a fixed number of columns */
struct numratios { char check[LENGTH(column_ratios) != PosLast ? -1 : 1]; };

void
dir_set_curline(int *cl, size_t cf)
{
	*cl = MIN(cf, (size_t)wins[PosLeft].h);
}

void
on_resize(struct tb_event *ev)
{
	size_t i;
	int acc, width;
	unsigned sum;

	for (sum = i = 0; i < LENGTH(column_ratios); ++i)
		sum += column_ratios[i];

	for (acc = 0, i = 0; i < LENGTH(column_ratios) - 1; ++i, acc += width) {
		width = (float)column_ratios[i] / sum * ev->w;
		win_resize(&wins[i], acc, 0, width - 1, ev->h - 1);
	}
	win_resize(&wins[PosRight], acc, 0, ev->w - acc - 1, ev->h - 1);
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

	for (i = dir->cf - dir->cl, y = 0; y <= win->h && i < dir->fi.size; ++y, ++i)
		wprint(win, 1, y, 0, 0, dir->fi.arr[i].name);
	wprint(win, 1, dir->cl, 0, TB_REVERSE, dir->fi.arr[dir->cf].name);
}

void
screenredraw(void)
{
	int i;

	tb_clear();

	for (i = PosLeft; i < PosLast; ++i)
		wprintdir(&wins[i], dirs[i]);

	tb_present();
}

void
move_v(union arg *arg)
{
	size_t scf = dirs[PosMid]->cf;

	if (arg->i > 0)
		FILE_DOWN(dirs[PosMid], PREFIX_MULT(arg->i));
	else
		FILE_UP(dirs[PosMid], PREFIX_MULT(- arg->i));

	if (dirs[PosMid]->cf != scf) {
		dirs[PosMid]->cl += dirs[PosMid]->cf - scf;
		if (dirs[PosMid]->cl > wins[PosLeft].h)
			dirs[PosMid]->cl = wins[PosMid].h;
		else if (dirs[PosMid]->cl < 0)
			dirs[PosMid]->cl = 0;
		dirs[PosRight] = dir_child(dirs[PosMid]);
		screenredraw();
	}
}

void
move_h(union arg *arg)
{
	struct dir *sd = dirs[PosMid];

	if (arg->i > 0) {
		for (prefix = PREFIX_MULT(arg->i); dirs[PosRight] != NULL
		     && !DIR_IS_FAIL(dirs[PosRight]) && prefix > 0; --prefix) {
			dirs[PosLeft] = dirs[PosMid];
			dirs[PosMid] = dirs[PosRight];
			dirs[PosRight] = dir_child(dirs[PosMid]);
		}
	} else {
		for (prefix = PREFIX_MULT(- arg->i); dirs[PosLeft] != NULL
		     && prefix > 0; --prefix) {
			dirs[PosRight] = dirs[PosMid];
			dirs[PosMid] = dirs[PosLeft];
			dirs[PosLeft] = dir_parent(dirs[PosMid]);
		}
	}

	if (dirs[PosMid] != sd)
		screenredraw();
}

void
move_page(union arg *arg)
{
	union arg a;

	a.i = (int)(wins[PosMid].h * arg->f);
	move_v(&a);
}

void
goto_dir(union arg *arg)
{
	struct dir *dir;

	if ((dir = dir_get(arg->v)) == NULL)
		return;

	dirs[PosMid] = dir;
	dirs[PosLeft] = dir_parent(dir);
	dirs[PosRight] = dir_child(dir);

	screenredraw();
}

void
goto_line(union arg *arg)
{
	prefix += arg->u;
	if (prefix < dirs[PosMid]->fi.size)
		dirs[PosMid]->cf = prefix;
	else
		dirs[PosMid]->cf = dirs[PosMid]->fi.size - 1;
	dir_set_curline(&dirs[PosMid]->cl, dirs[PosMid]->cf);
	screenredraw();
}

void
sort_files(union arg *arg)
{
	int i;

	fs_set_sort(arg->u);
	for (i = PosLeft; i < PosLast; ++i)
		if (dirs[i] != NULL)
			dir_sort(dirs[i]);
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
	fs_set_sort(sort_code);

	dirs[PosMid] = dir_cwd();
	dirs[PosLeft] = dir_parent(dirs[PosMid]);
	dirs[PosRight] = dir_child(dirs[PosMid]);
}

void
run(void)
{
	struct tb_event ev;

	screenredraw();
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
