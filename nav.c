#include <dirent.h>
#include <string.h>
#include <errno.h>

#include "util.h"
#include "nav.h"
#include "hash.h"

#define FI_INIT_SIZE	(1U << 4)
#define FS_INIT_SIZE	4

#define IS_DOT(X)	(X[0] == '.' && (!X[1] || (X[1] == '.' && !X[2])))
#define IS_ROOT(X)	(X[0] == '/' && !X[1])
#define CMP_FILE(a, b)	((a).st_ino == (b).st_ino && (a).st_dev == (b).st_dev)
#define HIKEY2DIR(k)	((struct dir *)((k) - offsetof(struct dir, path)))
#define QSORT_DIR(d, f) (qsort((d)->fi_all, (d)->size_all, \
			       sizeof(struct file), (f)))

#define FLAG_ICASE	(1 << 0)
#define FLAG_FDIR	(1 << 1)
#define FLAG_SHOWHID	(1 << 2)

static struct dir * _dir_get(const char *, size_t);
static struct dir * dir_create(const char *, size_t);
static void dir_free(struct dir *);
static void dir_update_cf(struct dir *, struct stat *);
extern void dir_set_curline(int *, size_t);

static void dir_loadfiles(struct dir *);
static void dir_sort(struct dir *);
static void dir_filter(struct dir *);

static int cmp_name(const void *, const void *);
static int cmp_name_ci(const void *, const void *);
static int cmp_size(const void *, const void *);
static int cmp_dir(const void *, const void *);

static struct htable htdir;
static int (*compare)(const void *, const void *) = cmp_name;
static unsigned flags;
#define SET_FLAGBYVAL(f, v)	(flags = ((v) == 0) ? flags & ~(f) : flags | (f))

void
nav_init(void)
{
	ht_init(&htdir, FS_INIT_SIZE);
}

void
nav_clean(void)
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
nav_set_sort(unsigned c)
{
	switch (c) {
	case SORTBY_NAME:
		compare = cmp_name;
		break;
	case SORTBY_SIZE:
		compare = cmp_size;
		break;
	}
}

void
nav_set_caseins(unsigned c)
{
	SET_FLAGBYVAL(FLAG_ICASE, c);
}

void
nav_toggle_caseins(void)
{
	flags ^= FLAG_ICASE;
}

void
nav_set_dirfirst(unsigned c)
{
	SET_FLAGBYVAL(FLAG_FDIR, c);
}

void
nav_toggle_dirfirst(void)
{
	flags ^= FLAG_FDIR;
}

void
nav_set_showhid(unsigned c)
{
	SET_FLAGBYVAL(FLAG_SHOWHID, c);
}

void
nav_toggle_showhid(void)
{
	flags ^= FLAG_SHOWHID;
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
		dir_refilter(dir);
	}

	return dir;
}

struct dir *
dir_get(const char *path)
{
	return _dir_get(path, strlen(path));
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

	if (dir->cf < dir->size && CMP_FILE(dir->fi[dir->cf]->st, *st))
		return;

	for (dir->cf = i = 0; i < dir->size; ++i) {
		if (CMP_FILE(dir->fi[i]->st, *st)) {
			dir->cf = i;
			break;
		}
	}
	dir_set_curline(&dir->cl, dir->cf);
}

struct dir *
dir_child(const struct dir *dir)
{
	struct dir *chi;
	size_t siz;

	if (DIR_IS_FAIL(dir) || !S_ISDIR(dir->fi[dir->cf]->st.st_mode))
		return NULL;

	siz = strlen(dir->fi[dir->cf]->name);
	if (IS_ROOT(dir->path)) {
		buffer[0] = '/';
		memcpy(buffer + 1, dir->fi[dir->cf]->name, siz + 1);
	} else {
		memcpy(buffer, dir->path, dir->plen);
		buffer[dir->plen] = '/';
		memcpy(buffer + dir->plen + 1, dir->fi[dir->cf]->name, ++siz);
	}

	chi = _dir_get(buffer, dir->plen + siz);

	return chi;
}

int
cmp_name(const void *a, const void *b)
{
	return strcmp(((struct file *)a)->name, ((struct file *)b)->name);
}

int
cmp_name_ci(const void *a, const void *b)
{
	return strcasecmp(((struct file *)a)->name, ((struct file *)b)->name);
}

int
cmp_size(const void *a, const void *b)
{
	struct file *fa = (struct file *)a;
	struct file *fb = (struct file *)b;

	if (fa->st.st_size < fb->st.st_size)
		return 1;
	else if (fa->st.st_size > fb->st.st_size)
		return -1;

	return 0;
}

int
cmp_dir(const void *a, const void *b)
{
	if (S_ISDIR(((struct file *)b)->st.st_mode)
	    && !S_ISDIR(((struct file *)a)->st.st_mode))
		return 1;

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
	if (compare == cmp_name && flags & FLAG_ICASE)
		QSORT_DIR(dir, cmp_name_ci);
	else
		QSORT_DIR(dir, compare);

	if (flags & FLAG_FDIR)
		QSORT_DIR(dir, cmp_dir);

	dir->cmp = compare;
	dir->flags = flags;
}

void
dir_resort(struct dir *dir)
{
	struct stat st;

	if (DIR_IS_FAIL(dir) || (dir->cmp == compare
				 && dir->flags == flags))
		return;

	st = dir->fi[dir->cf]->st;

	dir_sort(dir);
	dir_filter(dir);

	dir_update_cf(dir, &st);
}

void
dir_filter(struct dir *dir)
{
	size_t i;

	/* TODO: implement custom filters. */

	if (flags & FLAG_SHOWHID) {
		for (i = 0; i < dir->size_all; ++i)
			dir->fi[i] = &dir->fi_all[i];
		dir->size = dir->size_all;
	} else {
		for (dir->size = i = 0; i < dir->size_all; ++i)
			if (dir->fi_all[i].name[0] != '.')
				dir->fi[dir->size++] = &dir->fi_all[i];
	}

	dir->flags = flags;
}

void
dir_refilter(struct dir *dir)
{
	struct stat st;

	if (DIR_IS_FAIL(dir)
	    || (flags & FLAG_SHOWHID) == (dir->flags & FLAG_SHOWHID))
		return;

	st = dir->fi[dir->cf]->st;

	dir_filter(dir);

	dir_update_cf(dir, &st);
}

void
dir_loadfiles(struct dir *dir)
{
	DIR *ds;
	struct dirent *de;
	size_t siz;

	dir->size_all = 0;

	if ((ds = opendir(dir->path)) == NULL) {
		dir->fi_all = NULL;
		dir->fi = NULL;
		dir->size = 0;
		dir->u.err = errno;
		return;
	}

	dir->fi_all = sfm_malloc(FI_INIT_SIZE * sizeof(struct file));
	dir->u.cap = FI_INIT_SIZE;

	memcpy(buffer, dir->path, dir->plen);
	buffer[dir->plen] = '/';

	while ((de = readdir(ds)) != NULL) {
		if (IS_DOT(de->d_name))
			continue;

		if (dir->size_all == dir->u.cap) {
			dir->u.cap *= 2;
			dir->fi_all = sfm_realloc(dir->fi_all, dir->u.cap *
						  sizeof(struct file));
		}

#define lastfi		(dir->fi_all[dir->size_all])

		siz = strlen(de->d_name) + 1;
		lastfi.name = sfm_strndup(de->d_name, siz);

		memcpy(buffer + dir->plen + 1, de->d_name, siz);
		lstat(buffer, &lastfi.st);
		if (S_ISLNK(lastfi.st.st_mode)) {
			if (stat(buffer, &lastfi.st) != 0) {
				lastfi.realpath = lastfi.name;
			} else {
				lastfi.realpath = realpath(buffer, NULL);
			}
		} else {
			lastfi.realpath = NULL;
		}

		dir->size_all++;
#undef lastfi
	}

	closedir(ds);

	dir_sort(dir);

	dir->fi = sfm_malloc(dir->size_all * sizeof(struct file *));
	dir_filter(dir);
}

void
dir_free(struct dir *dir)
{
	size_t i;

	for (i = 0; i < dir->size_all; ++i) {
		if (dir->fi_all[i].realpath != dir->fi_all[i].name)
			free(dir->fi_all[i].realpath);
		free(dir->fi_all[i].name);
	}
	free(dir->fi_all);
	free(dir->fi);
	free(dir);
}
