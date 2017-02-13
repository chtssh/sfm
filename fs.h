#ifndef FS_H
#define FS_H

#include <sys/stat.h>

struct file {
	struct stat st;
	char *name;
};

struct arr_fi {
	struct file *arr;
	size_t size;
	union {
		size_t cap;
		int err;
	} u;
};

struct dir {
	struct stat st;
	struct arr_fi fi;
	size_t plen;
	size_t cf;
	int cl;			/* current line: rw field for TUI */
	char path[1];
};

#define DIR_IS_FAIL(d)	((d)->fi.arr == NULL)
#define DIR_IS_EMPTY(d)	((d)->fi.size == 0)
#define DIR_ERROR(d)	((d)->fi.u.err)

#define FILE_UP(d, s) do { \
	(d)->cf = (d)->cf < (s) ? 0 : (d)->cf - (s); \
} while (0)

#define FILE_DOWN(d, s) do { \
	if (SIZE_MAX - (d)->cf < (s)) { \
		(d)->cf = (d)->fi.size - 1; \
	} else { \
		(d)->cf += (s); \
		if ((d)->cf >= (d)->fi.size) { \
			(d)->cf = (d)->fi.size - 1; \
		} \
	} \
} while (0)

void fs_init(void);
void fs_clean(void);
struct dir * dir_get(const char *);
struct dir * dir_cwd(void);
struct dir * dir_parent(const struct dir *);
struct dir * dir_child(const struct dir *);

#endif /* FS_H */
