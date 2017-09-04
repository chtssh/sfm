#include <unistd.h> /* getcwd */
#include <sys/select.h>
#include <signal.h>

#include "util.h"
#include "ui.h"
#include "nav.h"

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
static void winch_handler(int);

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

static int winch_fds[2];
static int runner = 1;
static size_t prefix;
static struct nav *nav;
static struct ui *ui;

#define PREFIX_MULT(i)	(prefix > 0 ? prefix * (i) : (size_t)(i))
#define MIN_COLS	2

#include "config.h"

struct numratios { char check[LENGTH(column_ratios) < MIN_COLS ? -1 : 1]; };

void
move_v(union arg *arg)
{

	size_t step;

	if (arg->i > 0) {
		step = nav_move_down(nav, PREFIX_MULT(arg->i));
		ui_line_down(ui, step);
	} else {
		step = nav_move_up(nav, PREFIX_MULT(-(arg->i)));
		ui_line_up(ui, step);
	}

	if (step != 0) {
		ui_redraw(ui);
	}
}

void
move_h(union arg *arg)
{
	size_t step;

	if (arg->i > 0) {
		step = nav_move_forw(nav, PREFIX_MULT(arg->i));
	} else {
		step = nav_move_back(nav, PREFIX_MULT(-(arg->i)));
	}

	if (step != 0) {
		ui_redraw(ui);
	}
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
	nav_goto_dir(nav, (const char *)arg->v);

	ui_redraw(ui);
}

void
goto_line(union arg *arg)
{
	size_t line;
	union arg a;

	line = prefix + arg->u;
	if (line < prefix) {
		line = SIZE_MAX;
	}

	if (line > (*nav->main)->cf) {
		a.i = 1;
		prefix = line - (*nav->main)->cf;
	} else {
		a.i = -1;
		prefix = (*nav->main)->cf - line;
	}
	move_v(&a);
}

void
sortby(union arg *arg)
{
	if (nav_flag_edit(nav, ENABLE, arg->u)) {
		ui_redraw(ui);
	}
}

void
show_hid(union arg *arg)
{
	if (nav_flag_edit(nav, arg->i, NAV_SHOW_HIDDEN)) {
		ui_redraw(ui);
	}
}

void
sort_caseins(union arg *arg)
{
	if (nav_flag_edit(nav, arg->i, NAV_SORT_CASEINS)) {
		ui_redraw(ui);
	}
}

void
sort_dirfirst(union arg *arg)
{
	if (nav_flag_edit(nav, arg->i, NAV_SORT_DIRFIRST)) {
		ui_redraw(ui);
	}
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

	if (j == 1 && (*pkeys)->codes[ci] == '\0') {
		goto callf;
	} else if (j == 0) {
		goto reset;
	}

	/* TODO: print avalible keys */
	return;

callf:	(*pkeys)->fnc(&(*pkeys)->arg);
reset:	ci = prefix = 0;
}

void
winch_handler(int sig)
{
	write(winch_fds[1], &sig, sizeof(int));
}

void
setup(void)
{
	char *cwd;

	/* init fs */
	cwd = getcwd(buffer, PATH_MAX);

	nav = nav_create(cwd, LENGTH(column_ratios), sort |
			 (sort_case_insensitive ? NAV_SORT_CASEINS : 0) |
			 (sort_directories_first ? NAV_SORT_DIRFIRST : 0) |
			 (show_hidden ? NAV_SHOW_HIDDEN : 0));

	/* init ui */
	ui = ui_create(nav, column_ratios, LENGTH(column_ratios), &scheme);

	pipe(winch_fds);
	signal(SIGWINCH, winch_handler);
}

void
run(void)
{
	fd_set fdset;
	unsigned int *key;
	int winch_sig;

	ui_redraw(ui);

	while(runner) {
		FD_ZERO(&fdset);
		FD_SET(ui->fd, &fdset);
		FD_SET(winch_fds[0], &fdset);
		select(MAX(ui->fd, winch_fds[0]) + 1, &fdset, 0, 0, 0);

		if (FD_ISSET(ui->fd, &fdset)) {
			while ((key = ui_getch(ui)) != NULL) {
				on_keypress(*key);
			}
		}
		if (FD_ISSET(winch_fds[0], &fdset)) {
			winch_sig = 0;
			read(winch_fds[0], &winch_sig, sizeof(int));
			ui_on_resize(ui);
		}
	}
}

void
cleanup(void)
{
	ui_free(ui);
	nav_free(nav);
}

int
main(void)
{
	setup();
	run();
	return 0;
}
