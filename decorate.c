#include "cache.h"
#include "decorate.h"

void *add_decoration(struct decoration *n, const struct object *obj,
		     void *decoration)
{
	void *ret = NULL;
	map_set_object_void(&n->map, obj, decoration, &ret);
	return ret;
}

void *lookup_decoration(struct decoration *n, const struct object *obj)
{
	void *ret = NULL;
	map_get_object_void(&n->map, obj, &ret);
	return ret;
}
