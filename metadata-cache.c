#include "cache.h"
#include "metadata-cache.h"
#include "map.h"

static const char *metadata_cache_path(const char *name,
				       void (*validity)(unsigned char [20]))
{
	unsigned char token[20];

	if (validity)
		validity(token);
	else
		hashcpy(token, null_sha1);
	return git_path("cache/%s/%s", name, sha1_to_hex(token));
}

#define IMPLEMENT_METADATA_CACHE(name, map, validity) \
static struct map_persist_##map name##_map; \
static int name##_fd; \
static unsigned char *name##_buf; \
static unsigned long name##_len; \
\
static void write_##name##_cache(void) \
{ \
	const char *path; \
	struct strbuf tempfile = STRBUF_INIT; \
	int fd = -1; \
\
	if (!name##_map.mem.nr) \
		return; \
\
	path = metadata_cache_path(#name, validity); \
	strbuf_addf(&tempfile, "%s.XXXXXX", path); \
\
	if (safe_create_leading_directories(tempfile.buf) < 0) \
		goto fail; \
	fd = git_mkstemp_mode(tempfile.buf, 0444); \
	if (fd < 0) \
		goto fail; \
\
	if (write_in_full(fd, "MTAC\x00\x00\x00\x01", 8) < 0) \
		goto fail; \
	if (map_persist_flush_##map(&name##_map, fd) < 0) \
		goto fail; \
	if (close(fd) < 0) \
		goto fail; \
	if (rename(tempfile.buf, path) < 0) \
		goto fail; \
\
	strbuf_release(&tempfile); \
	return; \
\
fail: \
	close(fd); \
	unlink(tempfile.buf); \
	strbuf_release(&tempfile); \
} \
\
static void init_##name##_cache(void) \
{ \
	static int initialized; \
	const char *path; \
	struct stat sb; \
	const unsigned char *p; \
	uint32_t version; \
\
	if (initialized) \
		return; \
\
	atexit(write_##name##_cache); \
	initialized = 1; \
\
	path = metadata_cache_path(#name, validity); \
	name##_fd = open(path, O_RDONLY); \
	if (name##_fd < 0) \
		return; \
\
	if (fstat(name##_fd, &sb) < 0) \
		goto fail; \
	name##_len = sb.st_size; \
	name##_buf = xmmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, \
				 name##_fd, 0); \
\
	if (name##_len < 8) { \
		warning("cache '%s' is missing header", path); \
		goto fail; \
	} \
	p = name##_buf; \
	if (memcmp(p, "MTAC", 4)) { \
		warning("cache '%s' has invalid magic: %c%c%c%c", \
			path, p[0], p[1], p[2], p[3]); \
		goto fail; \
	} \
	p += 4; \
	memcpy(&version, p, 4); \
	version = ntohl(version); \
	if (version != 1) { \
		warning("cache '%s' has unknown version: %"PRIu32, \
			path, version); \
		goto fail; \
	} \
\
	map_persist_attach_##map(&name##_map, \
				     name##_buf + 8, \
				     name##_len - 8); \
	return; \
\
fail: \
	close(name##_fd); \
	name##_fd = -1; \
	if (name##_buf) \
		munmap(name##_buf, name##_len); \
	name##_buf = NULL; \
	name##_len = 0; \
} \
\
int name##_cache_get(map_ktype_##map key, map_vtype_##map *value) \
{ \
	init_##name##_cache(); \
	return map_persist_get_##map(&name##_map, key, value); \
} \
int name##_cache_set(map_ktype_##map key, map_vtype_##map value) \
{ \
	init_##name##_cache(); \
	return map_persist_set_##map(&name##_map, key, value); \
}
