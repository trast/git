#include "cache.h"
#include "dir.h"
#include "resolve-undo.h"
#include "string-list.h"

void resolve_undo_move_into_index(struct index_state *istate)
{
	struct string_list_item *item;

	if (istate->resolve_undo_state != RESOLVE_UNDO_SEPARATE)
		return;

	for_each_string_list_item(item, istate->resolve_undo) {
		struct resolve_undo_info *ru = item->util;
		int i;

		for (i = 0; i < 3; i++) {
			struct cache_entry *nce;
			if (!ru->mode[i])
				continue;
			nce = make_cache_entry(ru->mode[i], ru->sha1[i],
					       item->string, i + 1, 0);
			nce->ce_flags |= CE_REUC;
			if (add_index_entry(istate, nce, ADD_CACHE_OK_TO_ADD))
				die("cannot insert REUC '%s' into index", item->string);
		}
	}

	istate->resolve_undo_state = RESOLVE_UNDO_IN_INDEX;
}

int convert_to_resolve_undo(struct index_state *istate, struct cache_entry *ce)
{
	int stage = ce_stage(ce);

	if (!stage)
		return 0;

	assert(istate->resolve_undo_state != RESOLVE_UNDO_SEPARATE);
	istate->resolve_undo_state = RESOLVE_UNDO_IN_INDEX;
	istate->cache_changed = 1;

	ce->ce_flags |= CE_REUC;
	return 1;
}

static void resolve_undo_write_in_index(struct strbuf *sb, struct index_state *istate)
{
	int pos;
	for (pos = 0; pos < istate->cache_nr; pos++) {
		struct cache_entry *ce = istate->cache[pos];
		int i;
		unsigned int mode[3] = {0};
		unsigned char sha1[3][20];

		if (!ce_stage(ce) || !(ce->ce_flags & CE_REUC))
			continue;

		do {
			struct cache_entry *rce = istate->cache[pos];
			if (rce->ce_flags & CE_REUC) {
				mode[ce_stage(rce)-1] = rce->ce_mode;
				hashcpy(sha1[ce_stage(rce)-1], rce->sha1);
			}
			pos++;
		} while (pos < istate->cache_nr &&
		       !strcmp(ce->name, istate->cache[pos]->name));

		strbuf_addstr(sb, ce->name);
		strbuf_addch(sb, 0);
		for (i = 0; i < 3; i++)
			strbuf_addf(sb, "%o%c", mode[i], 0);
		for (i = 0; i < 3; i++) {
			if (!mode[i])
				continue;
			strbuf_add(sb, sha1[i], 20);
		}
	}
}

static void resolve_undo_write_separate(struct strbuf *sb, struct string_list *resolve_undo)
{
	struct string_list_item *item;
	for_each_string_list_item(item, resolve_undo) {
		struct resolve_undo_info *ui = item->util;
		int i;

		if (!ui)
			continue;
		strbuf_addstr(sb, item->string);
		strbuf_addch(sb, 0);
		for (i = 0; i < 3; i++)
			strbuf_addf(sb, "%o%c", ui->mode[i], 0);
		for (i = 0; i < 3; i++) {
			if (!ui->mode[i])
				continue;
			strbuf_add(sb, ui->sha1[i], 20);
		}
	}
}

void resolve_undo_write(struct strbuf *sb, struct index_state *istate)
{
	switch (istate->resolve_undo_state) {
	case RESOLVE_UNDO_NONE:
		return;
	case RESOLVE_UNDO_SEPARATE:
		resolve_undo_write_separate(sb, istate->resolve_undo);
		break;
	case RESOLVE_UNDO_IN_INDEX:
		resolve_undo_write_in_index(sb, istate);
		break;
	default:
		die("BUG: invalid resolve_undo_state");
	}
}

struct string_list *resolve_undo_read(const char *data, unsigned long size)
{
	struct string_list *resolve_undo;
	size_t len;
	char *endptr;
	int i;

	resolve_undo = xcalloc(1, sizeof(*resolve_undo));
	resolve_undo->strdup_strings = 1;

	while (size) {
		struct string_list_item *lost;
		struct resolve_undo_info *ui;

		len = strlen(data) + 1;
		if (size <= len)
			goto error;
		lost = string_list_insert(resolve_undo, data);
		if (!lost->util)
			lost->util = xcalloc(1, sizeof(*ui));
		ui = lost->util;
		size -= len;
		data += len;

		for (i = 0; i < 3; i++) {
			ui->mode[i] = strtoul(data, &endptr, 8);
			if (!endptr || endptr == data || *endptr)
				goto error;
			len = (endptr + 1) - (char*)data;
			if (size <= len)
				goto error;
			size -= len;
			data += len;
		}

		for (i = 0; i < 3; i++) {
			if (!ui->mode[i])
				continue;
			if (size < 20)
				goto error;
			hashcpy(ui->sha1[i], (const unsigned char *)data);
			size -= 20;
			data += 20;
		}
	}
	return resolve_undo;

error:
	string_list_clear(resolve_undo, 1);
	error("Index records invalid resolve-undo information");
	return NULL;
}

static void resolve_undo_clear_index_in_index(struct index_state *istate)
{
	int i;
	for (i = 0; i < istate->cache_nr; i++) {
		struct cache_entry *ce = istate->cache[i];
		if (ce->ce_flags & CE_REUC) {
			ce->ce_flags = (ce->ce_flags & ~CE_REUC) | CE_REMOVE;
			istate->cache_changed = 1;
		}
	}
}

static void resolve_undo_clear_index_separate(struct index_state *istate)
{
	struct string_list *resolve_undo = istate->resolve_undo;
	if (!resolve_undo)
		return;
	string_list_clear(resolve_undo, 1);
	free(resolve_undo);
	istate->resolve_undo = NULL;
	istate->cache_changed = 1;
}

void resolve_undo_clear_index(struct index_state *istate)
{
	switch (istate->resolve_undo_state) {
	case RESOLVE_UNDO_NONE:
		return;
	case RESOLVE_UNDO_SEPARATE:
		resolve_undo_clear_index_separate(istate);
		break;
	case RESOLVE_UNDO_IN_INDEX:
		resolve_undo_clear_index_in_index(istate);
		break;
	default:
		die("BUG: invalid resolve_undo_state");
	}
	istate->resolve_undo_state = RESOLVE_UNDO_NONE;
}

int unmerge_index_entry_at(struct index_state *istate, int pos)
{
	struct cache_entry *ce;
	int i, found = 0;

	if (istate->resolve_undo_state == RESOLVE_UNDO_NONE)
		return pos;
	assert (istate->resolve_undo_state == RESOLVE_UNDO_IN_INDEX);

	ce = istate->cache[pos];
	if (ce_stage(ce)) {
		/* already unmerged */
		while ((pos < istate->cache_nr) &&
		       ! strcmp(istate->cache[pos]->name, ce->name))
			pos++;
		return pos - 1; /* return the last entry processed */
	}

	for (i = pos+1; (i < istate->cache_nr) &&
		     !strcmp(istate->cache[i]->name, ce->name); i++) {
		struct cache_entry *unmerged = istate->cache[i];
		assert (ce_stage(unmerged));
		found = i;
		unmerged->ce_flags &= ~CE_REUC;
	}
	if (found) {
		remove_index_entry_at(istate, pos);
		return found-1;
	}
	return pos;
}

void unmerge_index(struct index_state *istate, const char **pathspec)
{
	int i;

	if (istate->resolve_undo_state == RESOLVE_UNDO_NONE)
		return;
	if (istate->resolve_undo_state == RESOLVE_UNDO_SEPARATE)
		resolve_undo_move_into_index(istate);

	for (i = 0; i < istate->cache_nr; i++) {
		struct cache_entry *ce = istate->cache[i];
		if (!match_pathspec(pathspec, ce->name, ce_namelen(ce), 0, NULL))
			continue;
		i = unmerge_index_entry_at(istate, i);
	}
}

void resolve_undo_convert_v5(struct index_state *istate,
					struct conflict_entry *ce)
{
	int i;

	while (ce) {
		struct string_list_item *lost;
		struct resolve_undo_info *ui;
		struct conflict_part *cp;

		if (ce->entries && (ce->entries->flags & CONFLICT_CONFLICTED) != 0) {
			ce = ce->next;
			continue;
		}
		if (!istate->resolve_undo) {
			istate->resolve_undo = xcalloc(1, sizeof(struct string_list));
			istate->resolve_undo->strdup_strings = 1;
		}

		lost = string_list_insert(istate->resolve_undo, ce->name);
		if (!lost->util)
			lost->util = xcalloc(1, sizeof(*ui));
		ui = lost->util;

		cp = ce->entries;
		for (i = 0; i < 3; i++)
			ui->mode[i] = 0;
		while (cp) {
			ui->mode[conflict_stage(cp) - 1] = cp->entry_mode;
			hashcpy(ui->sha1[conflict_stage(cp) - 1], cp->sha1);
			cp = cp->next;
		}
		ce = ce->next;
	}
}

void resolve_undo_to_ondisk_v5(struct string_list *resolve_undo,
				struct directory_entry *de)
{
	struct string_list_item *item;
	struct directory_entry *current;

	fprintf(stderr, "dirs:");
	for (current = de; current; current = current->next)
		fprintf(stderr, " '%s'", current->pathname);
	fprintf(stderr, "\n");

	if (!resolve_undo)
		return;
	for_each_string_list_item(item, resolve_undo) {
		struct conflict_entry *ce;
		struct resolve_undo_info *ui = item->util;
		char *super;
		int i;

		current = de;
		if (!ui)
			continue;

		super = super_directory(item->string);
		fprintf(stderr, "item: %s\n", item->string);
		while (super && current && strcmp(current->pathname, super) != 0) {
			fprintf(stderr, "skipped: %s\n", current->pathname);
			current = current->next;
		}
		if (!current)
			continue;
		fprintf(stderr, "found: %s\n", current->pathname);
		ce = xmalloc(conflict_entry_size(strlen(item->string)));
		ce->entries = NULL;
		ce->nfileconflicts = 0;
		ce->namelen = strlen(item->string);
		memcpy(ce->name, item->string, ce->namelen);
		ce->name[ce->namelen] = '\0';
		ce->pathlen = current->de_pathlen;
		fprintf(stderr, "path: %s name: %s len: %i\n", current->pathname, ce->name, ce->pathlen);
		if (ce->pathlen != 0)
			ce->pathlen++;
		current->de_ncr++;
		current->conflict_size += ce->namelen + 1;
		ce->next = NULL;
		for (i = 0; i < 3; i++) {
			if (ui->mode[i]) {
				struct conflict_part *cp, *cs;

				cp = xmalloc(sizeof(struct conflict_part));
				cp->flags = (i + 1) << CONFLICT_STAGESHIFT;
				cp->entry_mode = ui->mode[i];
				cp->next = NULL;
				hashcpy(cp->sha1, ui->sha1[i]);
				current->conflict_size += sizeof(struct ondisk_conflict_part);
				ce->nfileconflicts++;
				if (!ce->entries) {
					ce->entries = cp;
				} else {
					cs = ce->entries;
					while (cs->next)
						cs = cs->next;
					cs->next = cp;
				}
			}
		}
		if (current->conflict == NULL) {
			current->conflict = ce;
			current->conflict_last = current->conflict;
			current->conflict_last->next = NULL;
		} else {
			current->conflict_last->next = ce;
			current->conflict_last = current->conflict_last->next;
			current->conflict_last->next = NULL;
		}
	}
}
