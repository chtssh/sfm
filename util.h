#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#define LENGTH(X)	(sizeof X / sizeof X[0])

void die(const char *, ...) __attribute__((format(printf, 1, 2), noreturn));
void *sfm_malloc(size_t) __attribute__((malloc));
void *sfm_calloc(size_t, size_t) __attribute__((malloc));
void *sfm_realloc(void *, size_t);
char *sfm_strdup(const char *);
char *sfm_strndup(const char *, size_t);
size_t sfm_dirname(char *, const char *, size_t);

extern char buffer[PATH_MAX + NAME_MAX];

#endif /* UTIL_H */
