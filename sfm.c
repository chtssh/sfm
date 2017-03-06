#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "hash.h"
#include "ui.h"
#include "nav.h"

enum {
	TOGGLE = -1,
	FALSE,
	TRUE
};

union arg {
	int i;
	unsigned u;
	float f;
	void *v;
};

struct key {
	char *codes;
	void (*fnc)(union arg *);
	union arg arg;
};

struct schemes {
	struct clrscheme reg, dir, lnk, exe, chr, blk,
			 fifo, sock, lnkne, empty, err;
};

static void setup(void);
static void run(void);
static void cleanup(void) __attribute__((destructor));

static void move_v(union arg *);
static void move_h(union arg *);
static void move_page(union arg *);
static void goto_line(union arg *);
static void goto_dir(union arg *);
static void sortby(union arg *);
static void show_hid(union arg *);
static void sort_caseins(union arg *);
static void sort_dirfirst(union arg *);
static void quit(union arg *);

static int runner = 1;
static size_t prefix;

#define PREFIX_MULT(i)	(prefix > 0 ? prefix * (i) : (size_t)(i))
#define MIN_COLS	2

#include "config.h"

struct numratios { char check[LENGTH(column_ratios) < MIN_COLS ? -1 : 1]; };

static struct window wins[LENGTH(column_ratios)];
static struct dir *dirs[MAX(LENGTH(column_ratios), MIN_COLS + 1)];
#define dir_prev	(dirs[LENGTH(dirs) - 3])
#define dir_main	(dirs[LENGTH(dirs) - 2])
#define win_main	(wins[LENGTH(wins) - 2])
#define dir_preview	(dirs[LENGTH(dirs) - 1])
#define win_preview	(wins[LENGTH(wins) - 1])

void
dir_set_curline(int *cl, size_t cf)
{
	*cl = MIN(cf, (size_t)win_main.h);
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
	struct clrscheme cs;

	if (dir == NULL)
		return;

	if (DIR_IS_FAIL(dir))
		switch (DIR_ERROR(dir)) {
		case EACCES:
			wprint(win, 0, 0, &schemes.err,
			       "Permission denied.");
			return;
		case ENOENT:
			wprint(win, 0, 0, &schemes.err,
			       "Directory doesn't exist.");
			return;
		default:
			wprint(win, 0, 0, &schemes.err,
			       "Unknown error.");
			return;
		}

	if (DIR_IS_EMPTY(dir)) {
		wprint(win, 0, 0, &schemes.empty, "Empty.");
		return;
	}

	for (i = dir->cf - dir->cl, y = 0; y <= win->h && i < dir->size; ++y, ++i) {
#define curmode (dir->fi[i]->st.st_mode)

		if (dir->fi[i]->realpath == dir->fi[i]->name)
			cs = schemes.lnkne;
		else if (dir->fi[i]->realpath != NULL)
			cs = schemes.lnk;
		else if (S_ISREG(curmode))
			if (curmode & 0111)
				cs = schemes.exe;
			else
				cs = schemes.reg;
		else if (S_ISDIR(curmode))
			cs = schemes.dir;
		else if (S_ISCHR(curmode))
			cs = schemes.chr;
		else if (S_ISBLK(curmode))
			cs = schemes.blk;
		else if (S_ISFIFO(curmode))
			cs = schemes.fifo;
		else if (S_ISSOCK(curmode))
			cs = schemes.sock;
		else
			cs = schemes.reg;

		if (i == dir->cf)
			cs.fg |= TB_REVERSE;

		wprint(win, 1, y, &cs, dir->fi[i]->name);

#undef curmode
	}
}

void
screenredraw(void)
{
	size_t i;

	tb_clear();

	for (i = 0; i < LENGTH(wins); ++i)
		wprintdir(&wins[i], dirs[i + LENGTH(dirs) - LENGTH(wins)]);

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
		for (prefix = PREFIX_MULT(- arg->i); dir_prev != NULL
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
sortby(union arg *arg)
{
	size_t i;

	nav_set_sort(arg->u);
	for (i = 0; i < LENGTH(dirs); ++i)
		if (dirs[i] != NULL)
			dir_resort(dirs[i]);
	screenredraw();
}

void
show_hid(union arg *arg)
{
	size_t i;

	if (arg->i < 0)
		nav_toggle_showhid();
	else
		nav_set_showhid(arg->i);
	for (i = 0; i < LENGTH(dirs); ++i)
		if (dirs[i] != NULL)
			dir_refilter(dirs[i]);
	screenredraw();
}

void
sort_caseins(union arg *arg)
{
	size_t i;

	if (arg->i < 0)
		nav_toggle_caseins();
	else
		nav_set_caseins(arg->i);
	for (i = 0; i < LENGTH(dirs); ++i)
		if (dirs[i] != NULL)
			dir_resort(dirs[i]);
	screenredraw();
}

void
sort_dirfirst(union arg *arg)
{
	size_t i;

	if (arg->i < 0)
		nav_toggle_dirfirst();
	else
		nav_set_dirfirst(arg->i);
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

	if (ci == 0) {
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

	ci++;
	pkeys[j] = NULL;

	if (j == 1 && (*pkeys)->codes[ci] == '\0')
		goto callf;
	else if (j == 0)
		goto reset;

	/* TODO: print avalible keys */
	return;

callf:	(*pkeys)->fnc(&(*pkeys)->arg);
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
	nav_init();
	nav_set_sort(sort);
	nav_set_caseins(sort_case_insensitive);
	nav_set_dirfirst(sort_directories_first);
	nav_set_showhid(show_hidden);
}

void
run(void)
{
	union arg arg;
	struct tb_event ev;

	arg.v = getcwd(buffer, PATH_MAX);
	goto_dir(&arg);

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
	nav_clean();
	tb_shutdown();
}

int
main(void)
{
	setup();
	run();
	return 0;
}
