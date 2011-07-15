#ifndef MAP_H
#define MAP_H

#define DECLARE_MAP(name, key_type, value_type) \
typedef key_type map_ktype_##name; \
typedef value_type map_vtype_##name; \
struct map_entry_##name { \
	map_ktype_##name key; \
	map_vtype_##name value; \
	unsigned used:1; \
}; \
\
struct map_##name { \
	unsigned int size, nr; \
	struct map_entry_##name *hash; \
}; \
\
extern int map_get_##name(struct map_##name *, \
			  const map_ktype_##name key, \
			  map_vtype_##name *value); \
extern int map_set_##name(struct map_##name *, \
			  const map_ktype_##name key, \
			  map_vtype_##name value, \
			  map_vtype_##name *old);

#define DECLARE_MAP_PERSIST(name) \
struct map_persist_##name { \
	struct map_##name mem; \
	const unsigned char *disk_entries; \
	unsigned int disk_nr; \
}; \
extern int map_persist_get_##name(struct map_persist_##name *, \
			  const map_ktype_##name key, \
			  map_vtype_##name *value); \
extern int map_persist_set_##name(struct map_persist_##name *, \
			  const map_ktype_##name key, \
			  map_vtype_##name value); \
extern void map_persist_attach_##name(struct map_persist_##name *, \
				      const unsigned char *buf, \
				      unsigned int len); \
extern int map_persist_flush_##name(struct map_persist_##name *, int fd);

DECLARE_MAP(object_uint32, const struct object *, uint32_t)
DECLARE_MAP(object_void, const struct object *, void *)

#endif /* MAP_H */
