#ifndef FS_H
#define FS_H

#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>

struct file {
	struct stat st;
	char *name;
	char *realpath;
};

struct dir {
	struct stat st;
	struct file *fi_all;
	struct file **fi;
	size_t size_all;
	size_t size;
	union {
		size_t cap;
		int err;
	} u;
	size_t plen;
	size_t cf;
	int cl;			/* current line: rw field for TUI */
	int (*cmp)(const void *, const void *);
	unsigned flags;
	char path[1];
};

#define DIR_IS_FAIL(d)	((d)->fi == NULL)
#define DIR_IS_EMPTY(d)	((d)->size == 0)
#define DIR_ERROR(d)	((d)->u.err)

#define FILE_UP(d, s) do { \
	(d)->cf = (d)->cf < (s) ? 0 : (d)->cf - (s); \
} while (0)

#define FILE_DOWN(d, s) do { \
	if (SIZE_MAX - (d)->cf < (s)) { \
		(d)->cf = (d)->size - 1; \
	} else { \
		(d)->cf += (s); \
		if ((d)->cf >= (d)->size) { \
			(d)->cf = (DIR_IS_EMPTY((d))) ? 0 : (d)->size - 1; \
		} \
	} \
} while (0)

#define SORTBY_NAME	0
#define SORTBY_SIZE	1

void nav_init(void);
void nav_clean(void);

void nav_set_sort(unsigned);
void nav_set_caseins(unsigned);
void nav_toggle_caseins(void);
void nav_set_dirfirst(unsigned);
void nav_toggle_dirfirst(void);
void nav_set_showhid(unsigned);
void nav_toggle_showhid(void);

struct dir * dir_get(const char *);
struct dir * dir_parent(const struct dir *);
struct dir * dir_child(const struct dir *);
void dir_resort(struct dir *);
void dir_refilter(struct dir *);

#endif /* FS_H */
