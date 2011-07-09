#ifndef METADATA_CACHE_H
#define METADATA_CACHE_H

#include "map.h"

#define DECLARE_METADATA_CACHE(name, map) \
extern int name##_cache_get(map_ktype_##map key, map_vtype_##map *value); \
extern int name##_cache_set(map_ktype_##map key, map_vtype_##map value);

DECLARE_METADATA_CACHE(generations, object_uint32)

#endif /* METADATA_CACHE_H */
