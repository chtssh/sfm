#include <string.h>
#include <stdint.h>

#include "util.h"
#include "hash.h"

#define HT_MINSIZE	(1UL << 4)
#define HI_REMOVED	((const char *)ht)
#define INDEX_NONE	(ht->size)

static size_t ht_hash(const char *);

#ifdef HT_DEBUG
static size_t hstat_lookup;
static size_t hstat_perturb;
static size_t hstat_expand;
#endif

void
ht_init(struct htable *ht, unsigned size)
{
	if ((ht->size = 1UL << size) < HT_MINSIZE)
		ht->size = HT_MINSIZE;
	ht->arr = sfm_calloc(ht->size, sizeof(*ht->arr));
	ht->used = 0;
}

void
ht_resize(struct htable *ht)
{
	struct htable nht;
	size_t i, j;

	if (ht->size >= SIZE_MAX >> 1UL)
		nht.size = SIZE_MAX;
	else
		nht.size = ht->size << 1UL;

	nht.arr = sfm_calloc(nht.size, sizeof(*nht.arr));

	for (i = 0; i < ht->size; ++i) {
		if (ht->arr[i].key != NULL && ht->arr[i].key != HI_REMOVED) {
			j = ht->arr[i].hv & (nht.size - 1);
			while (nht.arr[j].key != NULL)
				if (++j == nht.size)
					j = 0;
			nht.arr[j].hv = ht->arr[i].hv;
			nht.arr[j].key = ht->arr[i].key;
		}
	}

	free(ht->arr);
	ht->arr = nht.arr;
	ht->size = nht.size;
#ifdef HT_DEBUG
	hstat_expand++;
#endif
}

void
ht_clean(struct htable *ht)
{
	free(ht->arr);
}

char *
ht_find(struct htable *ht, size_t i)
{
	return ht->arr[i].key == HI_REMOVED ? NULL : (char *)ht->arr[i].key;
}

void
ht_insert(struct htable *ht, size_t i, const char *key)
{
	ht->arr[i].key = key;
	if (++ht->used * 4 > ht->size * 3)
		ht_resize(ht);
}

void
ht_remove(struct htable *ht, size_t i)
{
	if (ht->arr[i].key != NULL && ht->arr[i].key != HI_REMOVED) {
		ht->used--;
		ht->arr[i].key = HI_REMOVED;
	}
}

size_t
ht_lookup(struct htable *ht, const char *key)
{
	size_t hv;
	size_t i;
	size_t empty;

#ifdef HT_DEBUG
	hstat_lookup++;
#endif

	hv = ht_hash(key);
	i = hv & (ht->size - 1);

	if (ht->arr[i].key == NULL) {
		ht->arr[i].hv = hv;
		return i;
	}

	if (ht->arr[i].key == HI_REMOVED)
		empty = i;
	else if (ht->arr[i].hv == hv && strcmp(ht->arr[i].key, key) == 0)
		return i;
	else
		empty = INDEX_NONE;

	for (;;) {
#ifdef HT_DEBUG
		hstat_perturb++;
#endif
		/* TODO: find a more effective step than 1 */
		if (++i == ht->size)
			i = 0;

		if (ht->arr[i].key == NULL) {
			if (empty != INDEX_NONE) {
				ht->arr[empty].hv = hv;
				return empty;
			}
			ht->arr[i].hv = hv;
			return i;
		}

		if (ht->arr[i].key == HI_REMOVED && empty == INDEX_NONE)
			empty = i;
		else if (ht->arr[i].hv == hv
			 && strcmp(ht->arr[i].key, key) == 0) {
			if (empty != INDEX_NONE) {
				ht->arr[empty].hv = hv;
				ht->arr[empty].key = ht->arr[i].key;
				ht->arr[i].key = HI_REMOVED;
				return empty;
			} else {
				return i;
			}
		}
	}
}

size_t
ht_hash(const char *key)
{
	size_t hv;

	for (hv = 5381; *key != '\0'; ++key)
		hv = ((hv << 5) + hv) + *key;

	return hv;
}

char *
ht_first(struct htable *ht, size_t *i)
{
	*i = 0;

	return ht_next(ht, i);
}

char *
ht_next(struct htable *ht, size_t *i)
{
	for (; *i < ht->size; (*i)++)
		if (ht->arr[*i].key != HI_REMOVED && ht->arr[*i].key != NULL)
			return (char *)ht->arr[(*i)++].key;

	return NULL;
}

void
ht_debug_msg(FILE *stream)
{
#ifdef HT_DEBUG
	fprintf(stream, "Hash table expands: %zu\n", hstat_expand);
	fprintf(stream, "Hash table lookups: %zu\n", hstat_lookup);
	fprintf(stream, "Hash table perturb loops: %zu\n", hstat_perturb);
	fprintf(stream, "Percentage of perturb loops: %zu\n",
		hstat_perturb * 100 / hstat_lookup);
#else
	(void)stream;
#endif
}
