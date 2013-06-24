#include <stdlib.h>

#include "cache.h"
#include "commit.h"
#include "tag.h"
#include "diff.h"
#include "revision.h"
#include "progress.h"
#include "list-objects.h"
#include "pack.h"
#include "pack-bitmap.h"

struct stored_bitmap {
	unsigned char sha1[20];
	struct ewah_bitmap *root;
	struct stored_bitmap *xor;
	int flags;
};

struct bitmap_index {
	struct ewah_bitmap *commits;
	struct ewah_bitmap *trees;
	struct ewah_bitmap *blobs;
	struct ewah_bitmap *tags;

	khash_sha1 *bitmaps;

	struct packed_git *pack;

	struct {
		struct object_array entries;
		khash_sha1 *map;
	} fake_index;

	struct bitmap *result;

	int entry_count;
	char pack_checksum[20];

	int version;
	unsigned loaded : 1,
			 native_bitmaps : 1,
			 has_hash_cache : 1;

	struct ewah_bitmap *(*read_bitmap)(struct bitmap_index *index);

	void *map;
	size_t map_size, map_pos;

	uint32_t *delta_hashes;
};

static struct bitmap_index bitmap_git;

static struct ewah_bitmap *
lookup_stored_bitmap(struct stored_bitmap *st)
{
	struct ewah_bitmap *parent;
	struct ewah_bitmap *composed;

	if (st->xor == NULL)
		return st->root;

	composed = ewah_pool_new();
	parent = lookup_stored_bitmap(st->xor);
	ewah_xor(st->root, parent, composed);

	ewah_pool_free(st->root);
	st->root = composed;
	st->xor = NULL;

	return composed;
}

static struct ewah_bitmap *
_read_bitmap(struct bitmap_index *index)
{
	struct ewah_bitmap *b = ewah_pool_new();
	int bitmap_size;

	bitmap_size = ewah_read_mmap(b,
		index->map + index->map_pos,
		index->map_size - index->map_pos);

	if (bitmap_size < 0) {
		error("Failed to load bitmap index (corruped?)");
		ewah_pool_free(b);
		return NULL;
	}

	index->map_pos += bitmap_size;
	return b;
}

static struct ewah_bitmap *
_read_bitmap_native(struct bitmap_index *index)
{
	struct ewah_bitmap *b = calloc(1, sizeof(struct ewah_bitmap));
	int bitmap_size;

	bitmap_size = ewah_read_mmap_native(b,
		index->map + index->map_pos,
		index->map_size - index->map_pos);

	if (bitmap_size < 0) {
		error("Failed to load bitmap index (corruped?)");
		free(b);
		return NULL;
	}

	index->map_pos += bitmap_size;
	return b;
}

static int load_bitmap_header(struct bitmap_index *index)
{
	struct bitmap_disk_header *header = (void *)index->map;

	if (index->map_size < sizeof(*header))
		return error("Corrupted bitmap index (missing header data)");

	if (memcmp(header->magic, BITMAP_MAGIC_PREFIX, sizeof(BITMAP_MAGIC_PREFIX)) != 0)
		return error("Corrupted bitmap index file (wrong header)");

	index->version = (int)ntohs(header->version);
	if (index->version != 2)
		return error("Unsupported version for bitmap index file (%d)", index->version);

	/* Parse known bitmap format options */
	{
		uint32_t flags = ntohs(header->options);

		if ((flags & BITMAP_OPT_FULL_DAG) == 0) {
			return error("Unsupported options for bitmap index file "
				"(Git requires BITMAP_OPT_FULL_DAG)");
		}

		if (flags & BITMAP_OPT_HASH_CACHE)
			index->has_hash_cache = 1;

		index->read_bitmap = &_read_bitmap;

		/*
		 * If we are in a little endian machine and the bitmap
		 * was written in LE, we can mmap it straight into memory
		 * without having to parse it
		 */
		if ((flags & BITMAP_OPT_LE_BITMAPS)) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
			index->native_bitmaps = 1;
			index->read_bitmap = &_read_bitmap_native;
#else
			die("The existing bitmap index is written in little-endian "
				"byte order and cannot be read in this machine.\n"
				"Please re-build the bitmap indexes locally.");
#endif
		}
	}

	index->entry_count = ntohl(header->entry_count);
	memcpy(index->pack_checksum, header->checksum, sizeof(header->checksum));
	index->map_pos += sizeof(*header);

	return 0;
}

static struct stored_bitmap *
store_bitmap(struct bitmap_index *index,
	const unsigned char *sha1,
	struct ewah_bitmap *bitmap,
	struct stored_bitmap *xor_with, int flags)
{
	struct stored_bitmap *stored;
	khiter_t hash_pos;
	int ret;

	stored = xmalloc(sizeof(struct stored_bitmap));
	stored->root = bitmap;
	stored->xor = xor_with;
	stored->flags = flags;
	memcpy(stored->sha1, sha1, 20);

	hash_pos = kh_put_sha1(index->bitmaps, stored->sha1, &ret);
	if (ret == 0) {
		error("Duplicate entry in bitmap index: %s", sha1_to_hex(sha1));
		return NULL;
	}

	kh_value(index->bitmaps, hash_pos) = stored;
	return stored;
}

static int
load_bitmap_entries_v2(struct bitmap_index *index)
{
	static const int MAX_XOR_OFFSET = 16;

	int i;
	struct stored_bitmap *recent_bitmaps[16];
	struct bitmap_disk_entry_v2 *entry;

	void *index_pos = index->map + index->map_size -
		(index->entry_count * sizeof(struct bitmap_disk_entry_v2));

	for (i = 0; i < index->entry_count; ++i) {
		int xor_offset, flags, ret;
		struct stored_bitmap *xor_bitmap = NULL;
		struct ewah_bitmap *bitmap = NULL;
		uint32_t bitmap_pos;

		entry = index_pos;
		index_pos += sizeof(struct bitmap_disk_entry_v2);

		bitmap_pos = ntohl(entry->bitmap_pos);
		xor_offset = (int)entry->xor_offset;
		flags = (int)entry->flags;

		if (index->native_bitmaps) {
			bitmap = calloc(1, sizeof(struct ewah_bitmap));
			ret = ewah_read_mmap_native(bitmap,
				index->map + bitmap_pos,
				index->map_size - bitmap_pos);
		} else {
			bitmap = ewah_pool_new();
			ret = ewah_read_mmap(bitmap,
				index->map + bitmap_pos,
				index->map_size - bitmap_pos);
		}

		if (ret < 0 || xor_offset > MAX_XOR_OFFSET || xor_offset > i) {
			return error("Corrupted bitmap pack index");
		}

		if (xor_offset > 0) {
			xor_bitmap = recent_bitmaps[(i - xor_offset) % MAX_XOR_OFFSET];

			if (xor_bitmap == NULL)
				return error("Invalid XOR offset in bitmap pack index");
		}

		recent_bitmaps[i % MAX_XOR_OFFSET] = store_bitmap(
			index, entry->sha1, bitmap, xor_bitmap, flags);
	}

	return 0;
}

static int load_bitmap_index(
	struct bitmap_index *index,
	const char *path,
	struct packed_git *packfile)
{
	int fd = git_open_noatime(path);
	struct stat st;

	if (fd < 0) {
		return -1;
	}

	if (fstat(fd, &st)) {
		close(fd);
		return -1;
	}

	index->map_size = xsize_t(st.st_size);
	index->map = xmmap(NULL, index->map_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	index->bitmaps = kh_init_sha1();
	index->pack = packfile;
	index->fake_index.map = kh_init_sha1();

	if (load_bitmap_header(index) < 0)
		return -1;

	if (index->has_hash_cache) {
		index->delta_hashes = index->map + index->map_pos;
		index->map_pos += (packfile->num_objects * sizeof(uint32_t));
	}

	if ((index->commits = index->read_bitmap(index)) == NULL ||
		(index->trees = index->read_bitmap(index)) == NULL ||
		(index->blobs = index->read_bitmap(index)) == NULL ||
		(index->tags = index->read_bitmap(index)) == NULL)
		return -1;

	if (load_bitmap_entries_v2(index) < 0)
		return -1;

	index->loaded = true;
	return 0;
}

char *pack_bitmap_filename(struct packed_git *p)
{
	char *idx_name;
	int len;

	len = strlen(p->pack_name) - strlen(".pack");
	idx_name = xmalloc(len + strlen(".bitmap") + 1);

	memcpy(idx_name, p->pack_name, len);
	memcpy(idx_name + len, ".bitmap", strlen(".bitmap") + 1);

	return idx_name;
}

int open_pack_bitmap(struct packed_git *p)
{
	char *idx_name;
	int ret;

	if (open_pack_index(p))
		die("failed to open pack %s", p->pack_name);

	idx_name = pack_bitmap_filename(p);
	ret = load_bitmap_index(&bitmap_git, idx_name, p);
	free(idx_name);

	return ret;
}

void prepare_bitmap_git(void)
{
	struct packed_git *p;

	if (bitmap_git.loaded)
		return;

	for (p = packed_git; p; p = p->next) {
		if (open_pack_bitmap(p) == 0)
			return;
	}
}

struct include_data {
	struct bitmap *base;
	struct bitmap *seen;
};

static inline int bitmap_position_extended(const unsigned char *sha1)
{
	struct object_array *array = &bitmap_git.fake_index.entries;
	struct object_array_entry *entry ;
	int bitmap_pos;

	khiter_t pos = kh_get_sha1(bitmap_git.fake_index.map, sha1);

	if (pos < kh_end(bitmap_git.fake_index.map)) {
		entry = kh_value(bitmap_git.fake_index.map, pos);

		bitmap_pos = (entry - array->objects);
		bitmap_pos += bitmap_git.pack->num_objects;

		return bitmap_pos;
	}

	return -1;
}

static int bitmap_position(const unsigned char *sha1)
{
	int pos = find_pack_entry_pos(sha1, bitmap_git.pack);
	return (pos >= 0) ? pos : bitmap_position_extended(sha1);
}

static int fake_index_add_object(struct object *object, const char *name)
{
	khiter_t hash_pos;
	int hash_ret;
	int bitmap_pos;

	struct object_array *array = &bitmap_git.fake_index.entries;
	struct object_array_entry *entry;

	hash_pos = kh_put_sha1(bitmap_git.fake_index.map, object->sha1, &hash_ret);
	if (hash_ret > 0) {
		add_object_array(object, name, array);
		entry = &array->objects[array->nr - 1];
		kh_value(bitmap_git.fake_index.map, hash_pos) = entry;
	} else {
		entry = kh_value(bitmap_git.fake_index.map, hash_pos);
	}

	bitmap_pos = (entry - array->objects);
	bitmap_pos += bitmap_git.pack->num_objects;

	return bitmap_pos;
}

static void show_object(struct object *object,
	const struct name_path *path, const char *last, void *data)
{
	struct bitmap *base = data;
	int bitmap_pos;

	bitmap_pos = bitmap_position(object->sha1);
	if (bitmap_pos < 0) {
		bitmap_pos = fake_index_add_object(object, path_name(path, last));
	}

	bitmap_set(base, bitmap_pos);
}

static void show_commit(struct commit *commit, void *data)
{
	/* Nothing to do here */
}

static int
add_to_include_set(struct include_data *data, const unsigned char *sha1, int bitmap_pos)
{
	khiter_t hash_pos;

	if (data->seen && bitmap_get(data->seen, bitmap_pos))
		return 0;

	if (bitmap_get(data->base, bitmap_pos))
		return 0;

	hash_pos = kh_get_sha1(bitmap_git.bitmaps, sha1);
	if (hash_pos < kh_end(bitmap_git.bitmaps)) {
		struct stored_bitmap *st = kh_value(bitmap_git.bitmaps, hash_pos);
		bitmap_or_inplace(data->base, lookup_stored_bitmap(st));
		return 0;
	}

	bitmap_set(data->base, bitmap_pos);
	return 1;
}

static int
should_include(struct commit *commit, void *_data)
{
	struct include_data *data = _data;
	int bitmap_pos;

	bitmap_pos = bitmap_position(commit->object.sha1);
	if (bitmap_pos < 0) {
		bitmap_pos = fake_index_add_object((struct object *)commit, "");
	}

	if (!add_to_include_set(data, commit->object.sha1, bitmap_pos)) {
		struct commit_list *parent = commit->parents;

		while (parent) {
			parent->item->object.flags |= SEEN;
			parent = parent->next;
		}

		return 0;
	}

	return 1;
}

static struct bitmap *
find_objects(
	struct rev_info *revs,
	struct object_list *roots,
	struct bitmap *seen)
{
	struct bitmap *base = NULL;
	bool needs_walk = false;

	struct object_list *not_mapped = NULL;

	/**
	 * Go through all the roots for the walk. The ones that have bitmaps
	 * on the bitmap index will be `or`ed together to form an initial
	 * global reachability analysis.
	 *
	 * The ones without bitmaps in the index will be stored in the
	 * `not_mapped_list` for further processing.
	 */
	while (roots) {
		struct object *object = roots->item;
		roots = roots->next;

		if (object->type == OBJ_COMMIT) {
			khiter_t pos = kh_get_sha1(bitmap_git.bitmaps, object->sha1);

			if (pos < kh_end(bitmap_git.bitmaps)) {
				struct stored_bitmap *st = kh_value(bitmap_git.bitmaps, pos);
				struct ewah_bitmap *or_with = lookup_stored_bitmap(st);

				if (base == NULL)
					base = ewah_to_bitmap(or_with);
				else
					bitmap_or_inplace(base, or_with);

				object->flags |= SEEN;
				continue;
			}
		}

		object_list_insert(object, &not_mapped);
	}

	/**
	 * Best case scenario: We found bitmaps for all the roots,
	 * so the resulting `or` bitmap has the full reachability analysis
	 */
	if (not_mapped == NULL)
		return base;

	roots = not_mapped;

	/**
	 * Let's iterate through all the roots that don't have bitmaps to
	 * check we can determine them to be reachable from the existing
	 * global bitmap.
	 *
	 * If we cannot find them in the existing global bitmap, we'll need
	 * to push them to an actual walk and run it until we can confirm
	 * they are reachable
	 */
	while (roots) {
		struct object *object = roots->item;
		int pos;

		roots = roots->next;
		pos = bitmap_position(object->sha1);

		if (pos < 0 || base == NULL || !bitmap_get(base, pos)) {
			object->flags &= ~UNINTERESTING;
			add_pending_object(revs, object, "");
			needs_walk = true;
		} else {
			object->flags |= SEEN;
		}
	}

	if (needs_walk) {
		struct include_data incdata;

		if (base == NULL)
			base = bitmap_new();

		incdata.base = base;
		incdata.seen = seen;

		revs->include_check = should_include;
		revs->include_check_data = &incdata;

		if (prepare_revision_walk(revs))
			die("revision walk setup failed");

		traverse_commit_list(revs, show_commit, show_object, base);
	}

	return base;
}

static void show_extended_objects(
	struct bitmap *objects,
	show_reachable_fn show_reach)
{
	struct object_array_entry *entries = bitmap_git.fake_index.entries.objects;
	unsigned int nr = bitmap_git.fake_index.entries.nr;
	unsigned int i;

	for (i = 0; i < nr; ++i) {
		struct object *obj;

		if (!bitmap_get(objects, bitmap_git.pack->num_objects + i))
			continue;

		obj = entries[i].item;
		show_reach(obj->sha1, obj->type, pack_name_hash(entries[i].name), 0, NULL, 0);
	}
}

static void show_objects_for_type(
	struct bitmap *objects,
	struct ewah_bitmap *type_filter,
	enum object_type object_type,
	show_reachable_fn show_reach)
{
	size_t pos = 0, i = 0;
	uint32_t offset;

	struct ewah_iterator it;
	eword_t filter;

	ewah_iterator_init(&it, type_filter);

	while (i < objects->word_alloc && ewah_iterator_next(&filter, &it)) {
		eword_t word = objects->words[i] & filter;

		for (offset = 0; offset < BITS_IN_WORD; ++offset) {
			const unsigned char *sha1;
			off_t pack_off;
			uint32_t hash = 0;

			if ((word >> offset) == 0)
				break;

			offset += __builtin_ctzll(word >> offset);

			sha1 = nth_packed_object_sha1(bitmap_git.pack, pos + offset);
			pack_off = nth_packed_object_offset(bitmap_git.pack, pos + offset);

			if (bitmap_git.delta_hashes)
				hash = ntohl(bitmap_git.delta_hashes[pos + offset]);

			show_reach(sha1, object_type, hash, 0, bitmap_git.pack, pack_off);
		}

		pos += BITS_IN_WORD;
		i++;
	}
}

int prepare_bitmap_walk(struct rev_info *revs, uint32_t *result_size)
{
	unsigned int i;
	unsigned int pending_nr = revs->pending.nr;
	unsigned int pending_alloc = revs->pending.alloc;
	struct object_array_entry *pending_e = revs->pending.objects;

	struct object_list *wants = NULL;
	struct object_list *haves = NULL;

	struct bitmap *wants_bitmap = NULL;
	struct bitmap *haves_bitmap = NULL;

	prepare_bitmap_git();

	if (!bitmap_git.loaded)
		return -1;

	revs->pending.nr = 0;
	revs->pending.alloc = 0;
	revs->pending.objects = NULL;

	for (i = 0; i < pending_nr; ++i) {
		struct object *object = pending_e[i].item;

		if (object->type == OBJ_NONE)
			parse_object(object->sha1);

		while (object->type == OBJ_TAG) {
			struct tag *tag = (struct tag *) object;

			if (object->flags & UNINTERESTING) {
				object_list_insert(object, &haves);
			} else {
				object_list_insert(object, &wants);
			}

			if (!tag->tagged)
				die("bad tag");
			object = parse_object(tag->tagged->sha1);
			if (!object)
				die("bad object %s", sha1_to_hex(tag->tagged->sha1));
		}

		if (object->flags & UNINTERESTING) {
			object_list_insert(object, &haves);
		} else {
			object_list_insert(object, &wants);
		}
	}

	if (wants == NULL) {
		/* we don't want anything! we're done! */
		return 0;
	}

	if (haves != NULL) {
		haves_bitmap = find_objects(revs, haves, NULL);
		reset_revision_walk();

		if (haves_bitmap == NULL)
			goto restore_revs;
	}

	wants_bitmap = find_objects(revs, wants, haves_bitmap);

	if (wants_bitmap == NULL) {
		bitmap_free(haves_bitmap);
		reset_revision_walk();
		goto restore_revs;
	}

	if (haves_bitmap) {
		bitmap_and_not_inplace(wants_bitmap, haves_bitmap);
	}

	bitmap_git.result = wants_bitmap;

	if (result_size) {
		*result_size = bitmap_popcount(wants_bitmap);
	}

	bitmap_free(haves_bitmap);
	return 0;

restore_revs:
	revs->pending.nr = pending_nr;
	revs->pending.alloc = pending_alloc;
	revs->pending.objects = pending_e;
	return -1;
}

void traverse_bitmap_commit_list(show_reachable_fn show_reachable)
{
	if (!bitmap_git.result)
		die("Tried to traverse bitmap commit without setting it up first");

	show_objects_for_type(bitmap_git.result, bitmap_git.commits, OBJ_COMMIT, show_reachable);
	show_objects_for_type(bitmap_git.result, bitmap_git.trees, OBJ_TREE, show_reachable);
	show_objects_for_type(bitmap_git.result, bitmap_git.blobs, OBJ_BLOB, show_reachable);
	show_objects_for_type(bitmap_git.result, bitmap_git.tags, OBJ_TAG, show_reachable);

	show_extended_objects(bitmap_git.result, show_reachable);

	bitmap_free(bitmap_git.result);
	bitmap_git.result = NULL;
}

struct bitmap_test_data {
	struct bitmap *base;
	struct progress *prg;
	size_t seen;
};

static void test_show_object(struct object *object,
	const struct name_path *path, const char *last, void *data)
{
	struct bitmap_test_data *tdata = data;
	int bitmap_pos;

	bitmap_pos = bitmap_position(object->sha1);
	if (bitmap_pos < 0) {
		die("Object not in bitmap: %s\n", sha1_to_hex(object->sha1));
	}

	bitmap_set(tdata->base, bitmap_pos);
	display_progress(tdata->prg, ++tdata->seen);
}

static void test_show_commit(struct commit *commit, void *data)
{
	struct bitmap_test_data *tdata = data;
	int bitmap_pos;

	bitmap_pos = bitmap_position(commit->object.sha1);
	if (bitmap_pos < 0) {
		die("Object not in bitmap: %s\n", sha1_to_hex(commit->object.sha1));
	}

	bitmap_set(tdata->base, bitmap_pos);
	display_progress(tdata->prg, ++tdata->seen);
}

void test_bitmap_walk(struct rev_info *revs)
{
	struct object *root;
	struct bitmap *result = NULL;
	khiter_t pos;
	size_t result_popcnt;
	struct bitmap_test_data tdata;

	prepare_bitmap_git();

	if (!bitmap_git.loaded) {
		die("failed to load bitmap indexes");
	}

	if (revs->pending.nr != 1) {
		die("only one bitmap can be tested at a time");
	}

	fprintf(stderr, "Bitmap v%d test (%d entries loaded)\n",
		bitmap_git.version, bitmap_git.entry_count);

	root = revs->pending.objects[0].item;
	pos = kh_get_sha1(bitmap_git.bitmaps, root->sha1);

	if (pos < kh_end(bitmap_git.bitmaps)) {
		struct stored_bitmap *st = kh_value(bitmap_git.bitmaps, pos);
		struct ewah_bitmap *bm = lookup_stored_bitmap(st);

		fprintf(stderr, "Found bitmap for %s. %d bits / %08x checksum\n",
			sha1_to_hex(root->sha1), (int)bm->bit_size, ewah_checksum(bm));

		result = ewah_to_bitmap(bm);
	}

	if (result == NULL) {
		die("Commit %s doesn't have an indexed bitmap", sha1_to_hex(root->sha1));
	}

	revs->tag_objects = 1;
	revs->tree_objects = 1;
	revs->blob_objects = 1;

	result_popcnt = bitmap_popcount(result);

	if (prepare_revision_walk(revs))
		die("revision walk setup failed");

	tdata.base = bitmap_new();
	tdata.prg = start_progress("Verifying bitmap entries", result_popcnt);
	tdata.seen = 0;

	traverse_commit_list(revs, &test_show_commit, &test_show_object, &tdata);

	stop_progress(&tdata.prg);

	if (bitmap_equals(result, tdata.base)) {
		fprintf(stderr, "OK!\n");
	} else {
		fprintf(stderr, "Mismatch!\n");
	}
}
