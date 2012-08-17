#ifndef DECORATE_H
#define DECORATE_H

#include "map.h"

struct decoration {
	char *name;
	struct map_object_void map;
};

extern void *add_decoration(struct decoration *n, const struct object *obj, void *decoration);
extern void *lookup_decoration(struct decoration *n, const struct object *obj);

#endif
