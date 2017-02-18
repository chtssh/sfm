#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#define LENGTH(X)	(sizeof X / sizeof X[0])

void die(const char *fmt, ...) __attribute__((format(printf, 1, 2), noreturn));
void *sfm_malloc(size_t size) __attribute__((malloc));
void *sfm_calloc(size_t nmemb, size_t size) __attribute__((malloc));
void *sfm_realloc(void *ptr, size_t size);
char *sfm_strdup(const char *str);
char *sfm_strndup(const char *str, size_t siz);
size_t sfm_dirname(char *dst, const char *src, size_t siz);

extern char buffer[PATH_MAX + NAME_MAX];

#endif /* UTIL_H */
