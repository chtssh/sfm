#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>

#include "util.h"
#include "fs.h"
#include "hash.h"

#define FI_INIT_SIZE	(1U << 4)
#define FS_INIT_SIZE	4

#define IS_DOT(X)	(X[0] == '.' && (!X[1] || (X[1] == '.' && !X[2])))
#define IS_ROOT(X)	(X[0] == '/' && !X[1])
#define CMP_FILE(a, b)	((a).st_ino == (b).st_ino && (a).st_dev == (b).st_dev)
#define HIKEY2DIR(k)	((struct dir *)((k) - offsetof(struct dir, path)))

static struct dir * _dir_get(const char *, size_t);
static struct dir * dir_create(const char *, size_t);
static void dir_free(struct dir *);
static void dir_update_cf(struct dir *, struct stat *);
extern void dir_set_curline(int *, size_t);

static void dir_loadfiles(struct dir *);
static void dir_sort(struct dir *);

static int sortby_name(const void *, const void *);
static int sortby_size(const void *, const void *);

static struct htable htdir;
static int (*sort_func)(const void *, const void *) = sortby_name;

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
		dir_free(dir);
	}
	ht_clean(&htdir);
}

void
fs_set_sort(unsigned code)
{
	switch (code) {
	case FICMP_BYNAME:
		sort_func = sortby_name;
		break;
	case FICMP_BYSIZE:
		sort_func = sortby_size;
		break;
	}
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
		dir_resort(dir);
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
	struct stat st;

	if (dir == NULL || IS_ROOT(dir->path))
		return NULL;

	plen = sfm_dirname(buffer, dir->path, dir->plen);
	par = _dir_get(buffer, plen);

	if (S_ISLNK(dir->st.st_mode))
		stat(dir->path, &st);
	else
		st = dir->st;

	if (!DIR_IS_FAIL(dir))
		dir_update_cf(par, &st);

	return par;
}

void
dir_update_cf(struct dir *dir, struct stat *st)
{
	size_t i;

	if (CMP_FILE(dir->fi[dir->size].st, *st))
		return;

	for (i = 0; i < dir->size; ++i)
		if (CMP_FILE(dir->fi[i].st, *st)) {
			dir->cf = i;
			break;
		}
	dir_set_curline(&dir->cl, dir->cf);
}

struct dir *
dir_child(const struct dir *dir)
{
	struct dir *chi;
	size_t siz;

	if (DIR_IS_FAIL(dir) || !S_ISDIR(dir->fi[dir->cf].st.st_mode))
		return NULL;

	siz = strlen(dir->fi[dir->cf].name);
	if (IS_ROOT(dir->path)) {
		buffer[0] = '/';
		memcpy(buffer + 1, dir->fi[dir->cf].name, siz + 1);
	} else {
		memcpy(buffer, dir->path, dir->plen);
		buffer[dir->plen] = '/';
		memcpy(buffer + dir->plen + 1, dir->fi[dir->cf].name, ++siz);
	}

	chi = _dir_get(buffer, dir->plen + siz);

	return chi;
}

int
sortby_name(const void *a, const void *b)
{
	return strcmp(((struct file *)a)->name, ((struct file *)b)->name);
}

int
sortby_size(const void *a, const void *b)
{
	struct file *fa = (struct file *)a;
	struct file *fb = (struct file *)b;

	if (fa->st.st_size < fb->st.st_size)
		return 1;
	else if (fa->st.st_size > fb->st.st_size)
		return -1;

	return 0;
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
	dir_loadfiles(dir);

	return dir;
}

void
dir_sort(struct dir *dir)
{
	qsort(dir->fi, dir->size, sizeof(struct file), sort_func);
	dir->sort_func = sort_func;
}

void
dir_resort(struct dir *dir)
{
	struct stat st;

	if (DIR_IS_FAIL(dir) || dir->sort_func == sort_func)
		return;

	st = dir->fi[dir->cf].st;
	dir_sort(dir);
	dir_update_cf(dir, &st);
}

void
dir_loadfiles(struct dir *dir)
{
	DIR *ds;
	struct dirent *de;
	size_t siz;

	dir->size = 0;

	if ((ds = opendir(dir->path)) == NULL) {
		dir->fi = NULL;
		dir->u.err = errno;
		return;
	}

	dir->fi = sfm_malloc(FI_INIT_SIZE * sizeof(struct file));
	dir->u.cap = FI_INIT_SIZE;

	memcpy(buffer, dir->path, dir->plen);
	buffer[dir->plen] = '/';

	while ((de = readdir(ds)) != NULL) {
		if (IS_DOT(de->d_name))
			continue;

		if (dir->size == dir->u.cap) {
			dir->u.cap *= 2;
			dir->fi = sfm_realloc(dir->fi, dir->u.cap *
						  sizeof(struct file));
		}

		siz = strlen(de->d_name) + 1;
		dir->fi[dir->size].name = sfm_strndup(de->d_name, siz);

		memcpy(buffer + dir->plen + 1, de->d_name, siz);
		stat(buffer, &dir->fi[dir->size++].st);
	}
	dir_sort(dir);

	closedir(ds);
}

void
dir_free(struct dir *dir)
{
	size_t i;

	for (i = 0; i < dir->size; ++i)
		free(dir->fi[i].name);
	free(dir->fi);
	free(dir);
}
