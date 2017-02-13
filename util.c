#include <stdarg.h>
#include <string.h>

#include "util.h"

void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);

	exit(1);
}

void *
sfm_malloc(size_t size)
{
	void *ptr;

	if ((ptr = malloc(size)) == NULL)
		die("sfm is out of memory!");

	return ptr;
}

void *
sfm_calloc(size_t nmemb, size_t size)
{
	void *ptr;

	if ((ptr = calloc(nmemb, size)) == NULL)
		die("sfm is out of memory!");

	return ptr;
}

void *
sfm_realloc(void *ptr, size_t size)
{
	if ((ptr = realloc(ptr, size)) == NULL)
		die("sfm is out of memory!");

	return ptr;
}

char *
sfm_strndup(const char *str, size_t siz)
{
	char *dst;

	dst = sfm_malloc(siz + 1);
	memcpy(dst, str, siz);
	dst[siz] = '\0';

	return dst;
}

char *
sfm_strdup(const char *str)
{
	char *dst;
	size_t siz;

	siz = strlen(str) + 1;
	dst = sfm_malloc(siz);
	memcpy(dst, str, siz);

	return dst;
}

size_t
sfm_dirname(char *dst, const char *src, size_t siz)
{
	const char *end;
	size_t len;

	for (end = src + siz - 1; end != src && *end != '/'; --end);
	len = end - src;
	if (end == src)
		dst[len++] = '/';
	else
		memcpy(dst, src, len);
	dst[len] = '\0';

	return len;
}
