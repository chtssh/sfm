#ifndef FS_H
#define FS_H

#include <sys/stat.h>

struct file {
	struct stat st;
	char *name;
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
	int (*sort_func)(const void *, const void *);
	unsigned showhid;
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

#define FICMP_BYNAME	0
#define FICMP_BYSIZE	1

void fs_init(void);
void fs_clean(void);
void fs_set_sort(unsigned);
void fs_toggle_showhid(void);
struct dir * dir_get(const char *);
struct dir * dir_cwd(void);
struct dir * dir_parent(const struct dir *);
struct dir * dir_child(const struct dir *);
void dir_resort(struct dir *);
void dir_refilter(struct dir *);

#endif /* FS_H */
