#include <unistd.h> /* getcwd */
#include <sys/select.h>

#include "util.h"
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
static struct ui *ui;

#define PREFIX_MULT(i)	(prefix > 0 ? prefix * (i) : (size_t)(i))
#define MIN_COLS	2

#include "config.h"

struct numratios { char check[LENGTH(column_ratios) < MIN_COLS ? -1 : 1]; };

static struct dir *dirs[MAX(LENGTH(column_ratios), MIN_COLS + 1)];
#define dir_prev	(dirs[LENGTH(dirs) - 3])
#define dir_main	(dirs[LENGTH(dirs) - 2])
#define dir_preview	(dirs[LENGTH(dirs) - 1])

void
dir_set_curline(int *cl, size_t cf)
{
	*cl = MIN(cf, (size_t)ui->win->h);
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
		if (dir_main->cl > ui->win->h)
			dir_main->cl = ui->win->h;
		else if (dir_main->cl < 0)
			dir_main->cl = 0;
		dir_preview = dir_child(dir_main);
		ui_redraw(ui);
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
		ui_redraw(ui);
}

void
move_page(union arg *arg)
{
	union arg a;

	a.i = (int)(ui->win->h * arg->f);
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

	ui_redraw(ui);
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
	ui_redraw(ui);
}

void
sortby(union arg *arg)
{
	size_t i;

	nav_set_sort(arg->u);
	for (i = 0; i < LENGTH(dirs); ++i)
		if (dirs[i] != NULL)
			dir_resort(dirs[i]);
	ui_redraw(ui);
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
	ui_redraw(ui);
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
	ui_redraw(ui);
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
	ui_redraw(ui);
}

void
quit(union arg *arg)
{
	(void)arg;
	runner = 0;
}

void
on_keypress(unsigned int code)
{
	static struct key *pkeys[LENGTH(keys)];
	static int ci;
	struct key *ptmp[LENGTH(keys)];
	size_t i, j = 0;

	if (code >= '0' && code <= '9') {
		prefix = prefix * 10 + code - '0';
		return;
	}

	if (ci == 0) {
		for (i = 0; i < LENGTH(keys); ++i)
			if ((unsigned int)keys[i].codes[ci] == code)
				pkeys[j++] = &keys[i];
	} else {
		for (i = 0; pkeys[i] != NULL; ++i)
			if ((unsigned int)pkeys[i]->codes[ci] == code)
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
	/* init fs */
	nav_init();
	nav_set_sort(sort);
	nav_set_caseins(sort_case_insensitive);
	nav_set_dirfirst(sort_directories_first);
	nav_set_showhid(show_hidden);

	/* init ui */
	ui = ui_create((unsigned int *)column_ratios, dirs, LENGTH(column_ratios), &scheme);
}

void
run(void)
{
	union arg arg;
	fd_set fdset;
	unsigned int *key;

	arg.v = getcwd(buffer, PATH_MAX);
	goto_dir(&arg);

	while(runner) {
		FD_ZERO(&fdset);
		FD_SET(ui->fd, &fdset);
		select(ui->fd + 1, &fdset, 0, 0, 0);

		if (FD_ISSET(ui->fd, &fdset)) {
			while ((key = ui_getch(ui)) != NULL) {
				on_keypress(*key);
			}
		}
	}
}

void
cleanup(void)
{
	nav_clean();
	ui_free(ui);
}

int
main(void)
{
	setup();
	run();
	return 0;
}
