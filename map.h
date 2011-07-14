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

#endif /* MAP_H */
