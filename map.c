#include "cache.h"
#include "map.h"
#include "object.h"
#include "sha1-lookup.h"

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

static void obj_to_disk(const struct object *obj, unsigned char *out)
{
	hashcpy(out, obj->sha1);
}

static void uint32_to_disk(uint32_t v, unsigned char *out)
{
	v = htonl(v);
	memcpy(out, &v, 4);
}

static void disk_to_uint32(const unsigned char *disk, uint32_t *out)
{
	memcpy(out, disk, 4);
	*out = ntohl(*out);
}

static const unsigned char *disk_lookup_sha1(const unsigned char *buf,
					     unsigned nr,
					     unsigned ksize, unsigned vsize,
					     const unsigned char *key)
{
	int pos;

	pos = sha1_entry_pos(buf, ksize + vsize, 0, 0, nr, nr, key);
	if (pos < 0)
		return NULL;
	return buf + (pos * (ksize + vsize)) + ksize;
}

static int merge_entries(int fd, int ksize, int vsize,
			 const unsigned char *left, unsigned nr_left,
			 const unsigned char *right, unsigned nr_right)
{
#define ADVANCE(name) \
	do { \
		name += ksize + vsize; \
		nr_##name--; \
	} while (0)
#define WRITE_ENTRY(name) \
	do { \
		if (write_in_full(fd, name, ksize + vsize) < 0) \
			return -1; \
		ADVANCE(name); \
	} while (0)

	while (nr_left && nr_right) {
		int cmp = memcmp(left, right, ksize);

		/* skip duplicates, preferring left to right */
		if (cmp == 0)
			ADVANCE(right);
		else if (cmp < 0)
			WRITE_ENTRY(left);
		else
			WRITE_ENTRY(right);
	}
	while (nr_left)
		WRITE_ENTRY(left);
	while (nr_right)
		WRITE_ENTRY(right);

#undef WRITE_ENTRY
#undef ADVANCE

	return 0;
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

#define IMPLEMENT_MAP_PERSIST(name, \
			      ksize, k_to_disk, \
			      vsize, v_to_disk, disk_to_v, \
			      disk_lookup_fun) \
int map_persist_get_##name(struct map_persist_##name *m, \
			   const map_ktype_##name key, \
			   map_vtype_##name *value) \
{ \
	unsigned char disk_key[ksize]; \
	const unsigned char *disk_value; \
\
	if (map_get_##name(&m->mem, key, value)) \
		return 1; \
\
	if (!m->disk_entries) \
		return 0; \
\
	k_to_disk(key, disk_key); \
	disk_value = disk_lookup_fun(m->disk_entries, m->disk_nr, \
				     ksize, vsize, disk_key); \
	if (disk_value) { \
		disk_to_v(disk_value, value); \
		return 1; \
	} \
\
	return 0; \
} \
\
int map_persist_set_##name(struct map_persist_##name *m, \
			   const map_ktype_##name key, \
			   map_vtype_##name value) \
{ \
	return map_set_##name(&m->mem, key, value, NULL); \
} \
\
static unsigned char *flatten_mem_entries_##name(struct map_persist_##name *m) \
{ \
	unsigned char *ret, *out; \
	int i, nr; \
\
	out = ret = xmalloc(m->mem.nr * (ksize + vsize)); \
	nr = 0; \
	for (i = 0; i < m->mem.size; i++) { \
		struct map_entry_##name *e = m->mem.hash + i; \
\
		if (!e->used) \
			continue; \
\
		if (nr == m->mem.nr) \
			die("BUG: map hash contained extra values"); \
\
		k_to_disk(e->key, out); \
		out += ksize; \
		v_to_disk(e->value, out); \
		out += vsize; \
	} \
\
	return ret; \
} \
\
void map_persist_attach_##name(struct map_persist_##name *m, \
			       const unsigned char *buf, \
			       unsigned int len) \
{ \
	m->disk_entries = buf; \
	m->disk_nr = len / (ksize + vsize); \
} \
\
static int keycmp_##name(const void *a, const void *b) \
{ \
	return memcmp(a, b, ksize); \
} \
\
int map_persist_flush_##name(struct map_persist_##name *m, int fd) \
{ \
	unsigned char *mem_entries; \
	int r; \
\
	mem_entries = flatten_mem_entries_##name(m); \
	qsort(mem_entries, m->mem.nr, ksize + vsize, keycmp_##name); \
\
	r = merge_entries(fd, ksize, vsize, \
			  mem_entries, m->mem.nr, \
			  m->disk_entries, m->disk_nr); \
	free(mem_entries); \
	return r; \
}

IMPLEMENT_MAP(object_uint32, obj_equal, hash_obj)
IMPLEMENT_MAP(object_void, obj_equal, hash_obj)
