#include <dirent.h>
#include <string.h>
#include <errno.h>

#include "util.h"
#include "nav.h"
#include "hash.h"

#define FI_INIT_SIZE	(1U << 4)
#define FS_INIT_SIZE	4
#define NAV_MIN_DIRS	3

#define IS_DOT(X)	(X[0] == '.' && (!X[1] || (X[1] == '.' && !X[2])))
#define IS_ROOT(X)	(X[0] == '/' && !X[1])
#define CMP_FILE(a, b)	((a).st_ino == (b).st_ino && (a).st_dev == (b).st_dev)
#define HIKEY2DIR(k)	((struct dir *)((k) - offsetof(struct dir, path)))
#define QSORT_DIR(d, f) (qsort((d)->fi_all, (d)->size_all, \
			       sizeof(struct file), (f)))
#define NAV_FLAG_ISSET(n,f) ((n)->flag & (f))

static struct dir * dir_create(const char *, size_t, unsigned int);
static void dir_loadfiles(struct dir *);
static void dir_sort(struct dir *dir, unsigned int flags);
static void dir_filter(struct dir *dir, unsigned int flags);
static void dir_update_cf(struct dir *, struct stat *);
static void dir_free(struct dir *);

static int cmp_name(const void *, const void *);
static int cmp_name_ci(const void *, const void *);
static int cmp_size(const void *, const void *);
static int cmp_dir(const void *, const void *);

static struct dir * nav_getparent(struct nav *, const struct dir *);
static struct dir * nav_getchild(struct nav *, const struct dir *);
static struct dir * nav_getdir(struct nav *, const char *, size_t);

static void nav_update(struct nav *nav);
static void dir_update(struct dir *dir, unsigned int flag);

static int nav_flag_set(struct nav *, unsigned int);
static int nav_flag_clear(struct nav *, unsigned int);

struct nav *
nav_create(const char *abspath, unsigned int num_dirs, unsigned int flag)
{
	struct nav *nav;

	nav = sfm_malloc(sizeof(struct nav));
	nav->tbl = sfm_malloc(sizeof(struct htable));

	ht_init(nav->tbl, FS_INIT_SIZE);
	nav->num_dirs = MAX(num_dirs, NAV_MIN_DIRS);
	nav->dir = sfm_malloc(sizeof(struct dir *) * nav->num_dirs);

	nav->prev = nav->dir + nav->num_dirs - 3;
	nav->main = nav->dir + nav->num_dirs - 2;
	nav->next = nav->dir + nav->num_dirs - 1;

	nav_flag_set(nav, flag);

	nav_goto_dir(nav, abspath);

	return nav;
}

void
nav_free(struct nav *nav)
{
	size_t i;
	char *ik;
	struct dir *dir;

	for (ik = ht_first(nav->tbl, &i); ik != NULL; ik = ht_next(nav->tbl, &i)) {
		dir = HIKEY2DIR(ik);
		dir_free(dir);
	}
	free(nav->dir);
	ht_clean(nav->tbl);
	free(nav->tbl);
	free(nav);
}

void
nav_goto_dir(struct nav *nav, const char *abspath)
{
	struct dir *dir;
	unsigned int i;

	if ((dir = nav_getdir(nav, abspath, strlen(abspath))) == NULL) {
		/* TODO: return error */
		return;
	}

	*nav->main = dir;
	*nav->next = nav_getchild(nav, dir);
	for (i = nav->num_dirs - 2; i-- != 0; ) {
		nav->dir[i] = nav_getparent(nav, nav->dir[i+1]);
	}
}

size_t
nav_move_up(struct nav *nav, size_t step)
{
	size_t real_step;

	if ((*nav->main)->cf < step) {
		if ((*nav->main)->cf == 0) {
			return 0;
		} else {
			real_step = (*nav->main)->cf;
			(*nav->main)->cf = 0;
		}
	} else {
		(*nav->main)->cf -= step;
		real_step = step;
	}

	*nav->next = nav_getchild(nav, *nav->main);
	return real_step;
}

size_t
nav_move_down(struct nav *nav, size_t step)
{
	size_t sum;
	size_t real_step;

	sum = (*nav->main)->cf + step;

	if (sum < (*nav->main)->cf || sum >= (*nav->main)->size) {
		real_step = (*nav->main)->size - (*nav->main)->cf - 1;
		(*nav->main)->cf = (*nav->main)->size - 1;
	} else {
		real_step = step;
		(*nav->main)->cf = sum;
	}

	*nav->next = nav_getchild(nav, *nav->main);
	return real_step;
}

size_t
nav_move_back(struct nav *nav, size_t step)
{
	size_t i;
	size_t j;

	for (i = 0; i != step && *nav->prev != NULL; ++i) {
		for (j = nav->num_dirs - 1; j != 0; --j) {
			nav->dir[j] = nav->dir[j-1];
		}
		nav->dir[0] = nav_getparent(nav, nav->dir[1]);
	}

	return i;
}

size_t
nav_move_forw(struct nav *nav, size_t step)
{
	size_t i;
	size_t j;

	for (i = 0; i != step && *nav->next != NULL
	     && !DIR_IS_FAIL(*nav->next); ++i)
	{
		for (j = 0; j < nav->num_dirs - 1; ++j) {
			nav->dir[j] = nav->dir[j+1];
		}
		*nav->next = nav_getchild(nav, *nav->main);
	}

	return i;
}

int
nav_flag_clear(struct nav *nav, unsigned int flag)
{
	unsigned int tmp;

	tmp = nav->flag & flag;
	if (tmp == 0) {
		return 0;
	}

	nav->flag ^= tmp;
	nav_update(nav);

	return 1;
}

int
nav_flag_set(struct nav *nav, unsigned int flag)
{
	unsigned int tmp;


	if ((flag & NAV_MASK_SORTBY) < NAV_SORTBY_LAST) {
		tmp = (nav->flag & ~NAV_MASK_SORTBY) | flag;
	} else {
		tmp = nav->flag | (flag & ~NAV_MASK_SORTBY);
	}

	if ((tmp ^ nav->flag) == 0) {
		return 0;
	}

	nav->flag = tmp;
	nav_update(nav);

	return 1;
}

int
nav_flag_edit(struct nav *nav, int cmd, unsigned int flag)
{
	switch (cmd) {
	case TOGGLE:
		if (NAV_FLAG_ISSET(nav, flag))
	case DISABLE:
			return nav_flag_clear(nav, flag);
		else
			/* fallthrough */
	case ENABLE:
			return nav_flag_set(nav, flag);
	default:
			return 0;
	}
}

void
nav_update(struct nav *nav)
{
	size_t i;

	for (i = 0; i < nav->num_dirs; ++i) {
		dir_update(nav->dir[i], nav->flag);
	}
}

void
dir_update(struct dir *dir, unsigned int flag)
{
	struct stat st;

	if (dir == NULL || DIR_IS_EMPTY(dir)) {
		return;
	}

	st = dir->fi[dir->cf]->st;
	if ((flag & NAV_MASK_SORT) != (dir->flag & NAV_MASK_SORT)) {
		dir_sort(dir, flag);
		dir_filter(dir, flag);
	} else if ((flag & NAV_SHOW_HIDDEN) != (dir->flag & NAV_SHOW_HIDDEN)) {
		dir_filter(dir, flag);
	}
	dir_update_cf(dir, &st);

	dir->flag = flag;
}

void
dir_sort(struct dir *dir, unsigned int flag)
{
	switch (flag & NAV_MASK_SORTBY) {
	case NAV_SORTBY_NAME:
		if (flag & NAV_SORT_CASEINS) {
			QSORT_DIR(dir, cmp_name_ci);
		} else {
			QSORT_DIR(dir, cmp_name);
		}
		break;
	case NAV_SORTBY_SIZE:
		QSORT_DIR(dir, cmp_size);
		break;
	default:
		break;
	}

	if (flag & NAV_SORT_DIRFIRST) {
		QSORT_DIR(dir, cmp_dir);
	}
}

void
dir_filter(struct dir *dir, unsigned int flag)
{
	size_t i;

	/* TODO: implement custom filters. */

	if (flag & NAV_SHOW_HIDDEN) {
		for (i = 0; i < dir->size_all; ++i)
			dir->fi[i] = &dir->fi_all[i];
		dir->size = dir->size_all;
	} else {
		for (dir->size = i = 0; i < dir->size_all; ++i)
			if (dir->fi_all[i].name[0] != '.')
				dir->fi[dir->size++] = &dir->fi_all[i];
	}
}

struct dir *
nav_getdir(struct nav *nav, const char *path, size_t plen)
{
	size_t hv;
	char *ik;
	struct dir *dir;

	hv = ht_lookup(nav->tbl, path);
	if ((ik = ht_find(nav->tbl, hv)) == NULL) {
		if ((dir = dir_create(path, plen, nav->flag)) == NULL)
			return dir;
		ht_insert(nav->tbl, hv, dir->path);
	} else {
		dir = HIKEY2DIR(ik);
		dir_update(dir, nav->flag);
	}

	return dir;
}

struct dir *
nav_getparent(struct nav *nav, const struct dir *dir)
{
	struct dir *par;
	size_t plen;
	struct stat st;

	if (dir == NULL || IS_ROOT(dir->path))
		return NULL;

	plen = sfm_dirname(buffer, dir->path, dir->plen);
	par = nav_getdir(nav, buffer, plen);

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
	dir->cl = -1;
}

struct dir *
nav_getchild(struct nav *nav, const struct dir *dir)
{
	struct dir *child;
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

	child = nav_getdir(nav, buffer, dir->plen + siz);

	return child;
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
dir_create(const char *path, size_t plen, unsigned int flag)
{
	struct dir *dir;

	dir = sfm_malloc(sizeof(struct dir) + plen);
	dir->plen = plen;
	memcpy(dir->path, path, dir->plen + 1);

	lstat(dir->path, &dir->st);
	dir->cf = 0;
	dir->cl = 0;
	dir->flag = flag;

	dir_loadfiles(dir);

	return dir;
}

void
dir_loadfiles(struct dir *dir)
{
	DIR *ds;
	struct dirent *de;
	size_t siz;
	struct file *fi;

	dir->size_all = 0;

	if ((ds = opendir(dir->path)) == NULL) {
		dir->fi_all = NULL;
		dir->fi = NULL;
		dir->size = 0;
		dir->cl = errno;
		return;
	}

	dir->cap = FI_INIT_SIZE;
	dir->fi_all = sfm_malloc(dir->cap * sizeof(struct file));

	memcpy(buffer, dir->path, dir->plen);
	buffer[dir->plen] = '/';

	while ((de = readdir(ds)) != NULL) {
		if (IS_DOT(de->d_name)) {
			continue;
		}

		if (dir->size_all == dir->cap) {
			dir->cap *= 2;
			dir->fi_all = sfm_realloc(dir->fi_all, dir->cap *
						  sizeof(struct file));
		}

		fi = dir->fi_all + dir->size_all;

		siz = strlen(de->d_name) + 1;
		fi->name = sfm_strndup(de->d_name, siz);

		memcpy(buffer + dir->plen + 1, de->d_name, siz);
		lstat(buffer, &fi->st);
		if (S_ISLNK(fi->st.st_mode)) {
			if (stat(buffer, &fi->st) != 0) {
				fi->realpath = fi->name;
			} else {
				fi->realpath = realpath(buffer, NULL);
			}
		} else {
			fi->realpath = NULL;
		}

		dir->size_all++;
	}

	closedir(ds);

	dir->fi = sfm_malloc(dir->size_all * sizeof(struct file *));

	dir_sort(dir, dir->flag);
	dir_filter(dir, dir->flag);
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
