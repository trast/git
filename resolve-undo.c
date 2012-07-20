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
					struct conflict_queue *cq)
{
	int i;

	while (cq->ce) {
		struct string_list_item *lost;
		struct resolve_undo_info *ui;
		struct conflict_entry *ce;
		struct conflict_part *cp;

		ce = cq->ce;
		if ((ce->entries->flags & CONFLICT_CONFLICTED) != 0) {
			cq = cq->next;
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
			ui->mode[conflict_stage(cp)] = cp->entry_mode;
			hashcpy(ui->sha1[i], cp->sha1);
			cp = cp->next;
		}
		cq = cq->next;
	}
}

void resolve_undo_to_ondisk_v5(struct string_list *resolve_undo,
				struct directory_entry *de)
{
	struct string_list_item *item;

	if (!resolve_undo)
		return;
	for_each_string_list_item(item, resolve_undo) {
		struct conflict_queue *cq;
		struct conflict_entry *ce;
		struct resolve_undo_info *ui = item->util;
		int i;

		if (!ui)
			continue;

		while (strcmp(de->pathname, super_directory(item->string)) != 0)
			de = de->next;
		cq = xmalloc(sizeof(struct conflict_queue));
		ce = xmalloc(conflict_entry_size(strlen(item->string)));
		ce->entries = NULL;
		ce->nfileconflicts = 0;
		ce->namelen = strlen(item->string);
		memcpy(ce->name, item->string, ce->namelen);
		ce->name[ce->namelen] = '\0';
		ce->pathlen = de->de_pathlen;
		de->de_ncr++;
		cq->next = NULL;
		for (i = 0; i < 3; i++) {
			if (ui->mode[i]) {
				struct conflict_part *cp, *cs;

				cp = xmalloc(sizeof(struct conflict_part));
				cp->flags = (i + 1) << CONFLICT_STAGESHIFT;
				cp->entry_mode = ui->mode[i];
				cp->next = NULL;
				hashcpy(cp->sha1, ui->sha1[i]);
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
		if (de->conflict == NULL) {
			de->conflict = cq;
			de->conflict_last = de->conflict;
			de->conflict_last->next = NULL;
		} else {
			de->conflict_last->next = cq;
			de->conflict_last = de->conflict_last->next;
			de->conflict_last->next = NULL;
		}
	}
}
