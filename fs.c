#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>

#include "util.h"
#include "fs.h"
#include "hash.h"

#define FI_INIT_SIZE	(1U << 4)
#define FS_INIT_SIZE	4

#define IS_DOT(X)	(X[0] == '.' && (!X[1] || (X[1] == '.' && !X[2])))
#define IS_ROOT(X)	(X[0] == '/' && !X[1])
#define CMP_FILE(a, b)	((a).st_ino == (b).st_ino && (a).st_dev == (b).st_dev)
#define HIKEY2DIR(k)	((struct dir *)((k) - offsetof(struct dir, path)))

static void fi_clean(struct arr_fi *);
static void fi_init(struct arr_fi *, const char *, size_t);
static struct dir * _dir_get(const char *, size_t);
static struct dir * dir_create(const char *, size_t);
static void dir_clean(struct dir *dir);
extern void dir_set_curline(int *, size_t);

static char buffer[PATH_MAX + NAME_MAX];
static struct htable htdir;

void
fs_init(void)
{
	ht_init(&htdir, FS_INIT_SIZE);
}

void
fs_clean(void)
{
	size_t i;
	char *ik;
	struct dir *dir;

	for (ik = ht_first(&htdir, &i); ik != NULL; ik = ht_next(&htdir, &i)) {
		dir = HIKEY2DIR(ik);
		dir_clean(dir);
		free(dir);
	}
	ht_clean(&htdir);
}

struct dir *
_dir_get(const char *path, size_t plen)
{
	size_t hv;
	char *ik;
	struct dir *dir;

	hv = ht_lookup(&htdir, path);
	if ((ik = ht_find(&htdir, hv)) == NULL) {
		if ((dir = dir_create(path, plen)) == NULL)
			return dir;
		ht_insert(&htdir, hv, dir->path);
	} else {
		dir = HIKEY2DIR(ik);
	}

	return dir;
}

struct dir *
dir_get(const char *path)
{
	return _dir_get(path, strlen(path));
}

struct dir *
dir_cwd(void)
{
	struct dir *dir;

	getcwd(buffer, PATH_MAX);
	dir = dir_get(buffer);

	return dir;
}

struct dir *
dir_parent(const struct dir *dir)
{
	struct dir *par;
	size_t plen;
	size_t i;
	struct stat st;

	if (IS_ROOT(dir->path))
		return NULL;

	plen = sfm_dirname(buffer, dir->path, dir->plen);
	par = _dir_get(buffer, plen);

	if (S_ISLNK(dir->st.st_mode))
		stat(dir->path, &st);
	else
		st = dir->st;

	if (!DIR_IS_FAIL(dir) && !CMP_FILE(par->fi.arr[par->fi.size].st, st)) {
		for (i = 0; i < par->fi.size; ++i) {
			if (CMP_FILE(par->fi.arr[i].st, st)) {
				par->cf = i;
				break;
			}
		}
		dir_set_curline(&par->cl, par->cf);
	}

	return par;
}

struct dir *
dir_child(const struct dir *dir)
{
	struct dir *chi;
	size_t siz;

	if (DIR_IS_FAIL(dir) || !S_ISDIR(dir->fi.arr[dir->cf].st.st_mode))
		return NULL;

	siz = strlen(dir->fi.arr[dir->cf].name);
	if (IS_ROOT(dir->path)) {
		buffer[0] = '/';
		memcpy(buffer + 1, dir->fi.arr[dir->cf].name, siz + 1);
	} else {
		memcpy(buffer, dir->path, dir->plen);
		buffer[dir->plen] = '/';
		memcpy(buffer + dir->plen + 1, dir->fi.arr[dir->cf].name, ++siz);
	}

	chi = _dir_get(buffer, dir->plen + siz);

	return chi;
}

struct dir *
dir_create(const char *path, size_t plen)
{
	struct dir *dir;

	dir = sfm_malloc(sizeof(struct dir) + plen);
	memcpy(dir->path, path, plen + 1);
	dir->plen = plen;
	dir->cf = dir->cl = 0;
	lstat(path, &dir->st);
	fi_init(&dir->fi, dir->path, dir->plen);

	return dir;
}

void
dir_clean(struct dir *dir)
{
	fi_clean(&dir->fi);
}

void
fi_init(struct arr_fi *fi, const char *path, size_t plen)
{
	DIR *ds;
	struct dirent *de;
	size_t siz;

	fi->size = 0;

	if ((ds = opendir(path)) == NULL) {
		fi->arr = NULL;
		fi->u.err = errno;
		return;
	}

	fi->arr = sfm_malloc(FI_INIT_SIZE * sizeof(struct file));
	fi->u.cap = FI_INIT_SIZE;

	memcpy(buffer, path, plen);
	buffer[plen++] = '/';

	while ((de = readdir(ds)) != NULL) {
		if (IS_DOT(de->d_name))
			continue;

		if (fi->size == fi->u.cap) {
			fi->u.cap *= 2;
			fi->arr = sfm_realloc(fi->arr,
					      fi->u.cap * sizeof(struct file));
		}

		siz = strlen(de->d_name) + 1;
		fi->arr[fi->size].name = sfm_strndup(de->d_name, siz);

		memcpy(buffer + plen, de->d_name, siz);
		stat(buffer, &fi->arr[fi->size++].st);
	}

	closedir(ds);
}

void
fi_clean(struct arr_fi *fi)
{
	size_t i;

	for (i = 0; i < fi->size; ++i)
		free(fi->arr[i].name);
}