#ifndef FS_H
#define FS_H

#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>

#define DIR_IS_FAIL(d)	((d)->fi == NULL)
#define DIR_IS_EMPTY(d)	((d)->size == 0)
#define DIR_ERROR(d)	((d)->cl)

#define NAV_SORTBY_NAME		0x0
#define NAV_SORTBY_SIZE		0x1
#define NAV_SORTBY_NATURAL	0x2
#define NAV_SORTBY_MTIME	0x3
#define NAV_SORTBY_CTIME	0x4
#define NAV_SORTBY_ATIME	0x5
#define NAV_SORTBY_RANDOM	0x6
#define NAV_SORTBY_TYPE		0x7
#define NAV_SORTBY_EXTENSION	0x8
#define NAV_SORTBY_LAST		0x9

#define NAV_MASK_SORTBY		0xF

#define NAV_SORT_CASEINS	0x10
#define NAV_SORT_DIRFIRST	0x20
#define NAV_SORT_REVERSE	0x40

#define NAV_MASK_SORT		0xFF

#define NAV_SHOW_HIDDEN		0x100

#define DISABLE	0
#define ENABLE	1
#define TOGGLE	2

struct file {
	struct stat st;
	char *name;
	char *realpath;
};

struct dir {
	struct stat st;

	struct file *fi_all;
	size_t size_all;

	struct file **fi;
	size_t size;

	size_t cap;

	size_t cf;
	int cl;			/* current line: rw field for TUI */
	unsigned int flag;

	unsigned int plen;
	char path[1];
};

struct nav {
	struct htable *tbl;

	struct dir **dir;
	unsigned int num_dirs;

	struct dir **prev;
	struct dir **main;
	struct dir **next;

	unsigned int flag;
};

struct nav * nav_create(const char *, unsigned int, unsigned int);
void nav_free(struct nav *);
void nav_goto_dir(struct nav *, const char *abspath);
size_t nav_move_down(struct nav *, size_t);
size_t nav_move_up(struct nav *, size_t);
size_t nav_move_forw(struct nav *, size_t);
size_t nav_move_back(struct nav *, size_t);
int nav_flag_edit(struct nav *, int, unsigned int);

#endif /* FS_H */
