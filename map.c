#include "cache.h"
#include "map.h"
#include "object.h"

static unsigned int hash_obj(const struct object *obj, unsigned int n)
{
	unsigned int hash;

	memcpy(&hash, obj->sha1, sizeof(unsigned int));
	return hash % n;
}

static int obj_equal(const struct object *a, const struct object *b)
{
	return a == b;
}

#define IMPLEMENT_MAP(name, equal_fun, hash_fun) \
static int map_insert_##name(struct map_##name *m, \
			     const map_ktype_##name key, \
			     map_vtype_##name value, \
			     map_vtype_##name *old) \
{ \
	unsigned int j; \
\
	for (j = hash_fun(key, m->size); m->hash[j].used; j = (j+1) % m->size) { \
		if (equal_fun(m->hash[j].key, key)) { \
			if (old) \
				*old = m->hash[j].value; \
			m->hash[j].value = value; \
			return 1; \
		} \
	} \
\
	m->hash[j].key = key; \
	m->hash[j].value = value; \
	m->hash[j].used = 1; \
	m->nr++; \
	return 0; \
} \
\
static void map_grow_##name(struct map_##name *m) \
{ \
	struct map_entry_##name *old_hash = m->hash; \
	unsigned int old_size = m->size; \
	unsigned int i; \
\
	m->size = (old_size + 1000) * 3 / 2; \
	m->hash = xcalloc(m->size, sizeof(*m->hash)); \
	m->nr = 0; \
\
	for (i = 0; i < old_size; i++) { \
		if (!old_hash[i].used) \
			continue; \
		map_insert_##name(m, old_hash[i].key, old_hash[i].value, NULL); \
	} \
	free(old_hash); \
} \
\
int map_set_##name(struct map_##name *m, \
		   const map_ktype_##name key, \
		   map_vtype_##name value, \
		   map_vtype_##name *old) \
{ \
	if (m->nr >= m->size * 2 / 3) \
		map_grow_##name(m); \
	return map_insert_##name(m, key, value, old); \
} \
\
int map_get_##name(struct map_##name *m, \
		   const map_ktype_##name key, \
		   map_vtype_##name *value) \
{ \
	unsigned int j; \
\
	if (!m->size) \
		return 0; \
\
	for (j = hash_fun(key, m->size); m->hash[j].used; j = (j+1) % m->size) { \
		if (equal_fun(m->hash[j].key, key)) { \
			*value = m->hash[j].value; \
			return 1; \
		} \
	} \
	return 0; \
}
