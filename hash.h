#ifndef HASH_H
#define HASH_H

struct htable {
	struct {
		size_t hv;
		const char *key;
	} *arr;
	size_t size;
	size_t used;
};

void ht_init(struct htable *, unsigned);
void ht_clean(struct htable *);

size_t ht_lookup(struct htable *, const char *);
char * ht_find(struct htable *, size_t);
void ht_insert(struct htable *, size_t, const char *);
char * ht_first(struct htable *, size_t *);
char * ht_next(struct htable *, size_t *);

void ht_debug_msg(FILE *stream);

#endif /* HASH_H */
