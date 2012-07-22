#include "cache.h"
#include "dir.h"
#include "resolve-undo.h"
#include "string-list.h"

/* The only error case is to run out of memory in string-list */
void record_resolve_undo(struct index_state *istate, struct cache_entry *ce)
{
	struct string_list_item *lost;
	struct resolve_undo_info *ui;
	struct string_list *resolve_undo;
	int stage = ce_stage(ce);

	if (!stage)
		return;

	if (!istate->resolve_undo) {
		resolve_undo = xcalloc(1, sizeof(*resolve_undo));
		resolve_undo->strdup_strings = 1;
		istate->resolve_undo = resolve_undo;
	}
	resolve_undo = istate->resolve_undo;
	lost = string_list_insert(resolve_undo, ce->name);
	if (!lost->util)
		lost->util = xcalloc(1, sizeof(*ui));
	ui = lost->util;
	hashcpy(ui->sha1[stage - 1], ce->sha1);
	ui->mode[stage - 1] = ce->ce_mode;
}

void resolve_undo_write(struct strbuf *sb, struct string_list *resolve_undo)
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

void resolve_undo_clear_index(struct index_state *istate)
{
	struct string_list *resolve_undo = istate->resolve_undo;
	if (!resolve_undo)
		return;
	string_list_clear(resolve_undo, 1);
	free(resolve_undo);
	istate->resolve_undo = NULL;
	istate->cache_changed = 1;
}

int unmerge_index_entry_at(struct index_state *istate, int pos)
{
	struct cache_entry *ce;
	struct string_list_item *item;
	struct resolve_undo_info *ru;
	int i, err = 0;

	if (!istate->resolve_undo)
		return pos;

	ce = istate->cache[pos];
	if (ce_stage(ce)) {
		/* already unmerged */
		while ((pos < istate->cache_nr) &&
		       ! strcmp(istate->cache[pos]->name, ce->name))
			pos++;
		return pos - 1; /* return the last entry processed */
	}
	item = string_list_lookup(istate->resolve_undo, ce->name);
	if (!item)
		return pos;
	ru = item->util;
	if (!ru)
		return pos;
	remove_index_entry_at(istate, pos);
	for (i = 0; i < 3; i++) {
		struct cache_entry *nce;
		if (!ru->mode[i])
			continue;
		nce = make_cache_entry(ru->mode[i], ru->sha1[i],
				       ce->name, i + 1, 0);
		if (add_index_entry(istate, nce, ADD_CACHE_OK_TO_ADD)) {
			err = 1;
			error("cannot unmerge '%s'", ce->name);
		}
	}
	if (err)
		return pos;
	free(ru);
	item->util = NULL;
	return unmerge_index_entry_at(istate, pos);
}

void unmerge_index(struct index_state *istate, const char **pathspec)
{
	int i;

	if (!istate->resolve_undo)
		return;

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

void resolve_undo_to_ondisk_v5(struct hash_table *table,
				struct string_list *resolve_undo,
				unsigned int *ndir, int *total_dir_len,
				struct directory_entry *de)
{
	struct string_list_item *item;
	struct directory_entry *search;

	if (!resolve_undo)
		return;
	for_each_string_list_item(item, resolve_undo) {
		struct conflict_entry *ce;
		struct resolve_undo_info *ui = item->util;
		char *super;
		int i, dir_len;
		uint32_t crc;
		struct directory_entry *found, *current, *x;

		if (!ui)
			continue;

		super = super_directory(item->string);
		if (!super)
			dir_len = 0;
		else
			dir_len = strlen(super);
		crc = crc32(0, (Bytef*)super, dir_len);
		found = lookup_hash(crc, table);
		current = NULL;
		x = NULL;
		
		while (!found) {
			struct directory_entry *insert, *new;

			new = init_directory_entry(super, dir_len);
			if (!current)
				current = new;
			else
				new->de_nsubtrees = 0;
			insert = (struct directory_entry *)insert_hash(crc, new, table);
			if (insert) {
				while (insert->next_hash)
					insert = insert->next_hash;
				insert->next_hash = new;
			}
			new->next = x;
			x = new;
			(*ndir)++;
			*total_dir_len += new->de_pathlen + 2;
			super = super_directory(super);
			if (!super)
				dir_len = 0;
			else
				dir_len = strlen(super);
			crc = crc32(0, (Bytef*)super, dir_len);
			found = lookup_hash(crc, table);
		}
		search = found;
		while (search->next_hash && strcmp(super, search->pathname) != 0)
			search = search->next_hash;
		if (search && !current)
			current = search;
		if (!search && !current)
			current = x;
		if (!super && x) {
			x->next = de->next;
			de->next = x;
			de->de_nsubtrees++;
		}
		/* else { */
			/* super = super_directory(super); */
			/* crc = crc32(0, (Bytef*)super, dir_len); */
			/* if (x) { */
			/* 	found = lookup_hash(crc, table); */
			/* 	search = found; */
			/* 	while (search->next_hash && strcmp(super, search->pathname) != 0) */
			/* 		search = search->next_hash; */
			/* 	if (search) { */
			/* 		x->next = search->next; */
			/* 		search->next = x; */
			/* 		search->de_nsubtrees++; */
			/* 	} */
			/* } */
		/* } */


		ce = xmalloc(conflict_entry_size(strlen(item->string)));
		ce->entries = NULL;
		ce->nfileconflicts = 0;
		ce->namelen = strlen(item->string);
		memcpy(ce->name, item->string, ce->namelen);
		ce->name[ce->namelen] = '\0';
		ce->pathlen = current->de_pathlen;
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
