#include "cache.h"
#include "column.h"
#include "string-list.h"

#define MODE(mode) ((mode) & COL_MODE)

/* Display without layout when COL_ENABLED is not set */
static void display_plain(const struct string_list *list,
			  const char *indent, const char *nl)
{
	int i;

	for (i = 0; i < list->nr; i++)
		printf("%s%s%s", indent, list->items[i].string, nl);
}

void print_columns(const struct string_list *list, unsigned int mode,
		   struct column_options *opts)
{
	const char *indent = "", *nl = "\n";
	int padding = 1, width = term_columns();

	if (!list->nr)
		return;
	if (opts) {
		if (opts->indent)
			indent = opts->indent;
		if (opts->nl)
			nl = opts->nl;
		if (opts->width)
			width = opts->width;
		padding = opts->padding;
	}
	if (width <= 1 || !(mode & COL_ENABLED)) {
		display_plain(list, indent, nl);
		return;
	}
	die("BUG: invalid mode %d", MODE(mode));
}
