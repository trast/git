#include "cache.h"
#include "column.h"
#include "string-list.h"
#include "parse-options.h"

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

struct colopt {
	enum {
		ENABLE,
		MODE,
		OPTION
	} type;
	const char *name;
	int value;
};

/*
 * Set COL_ENABLED and COL_ENABLED_SET. If 'set' is -1, check if
 * stdout is tty.
 */
static int set_enable_bit(unsigned int *mode, int set, int stdout_is_tty)
{
	if (set < 0) {	/* auto */
		if (stdout_is_tty < 0)
			stdout_is_tty = isatty(1);
		set = stdout_is_tty || (pager_in_use() && pager_use_color);
	}
	if (set)
		*mode = *mode | COL_ENABLED | COL_ENABLED_SET;
	else
		*mode = (*mode & ~COL_ENABLED) | COL_ENABLED_SET;
	return 0;
}

/*
 * Set COL_MODE_*. mode is intially copied from column.ui. If
 * COL_ENABLED_SET is not set, then neither 'always', 'never' nor
 * 'auto' has been used. Default to 'always'.
 */
static int set_mode(unsigned int *mode, unsigned int value)
{
	*mode = (*mode & ~COL_MODE) | value;
	if (!(*mode & COL_ENABLED_SET))
		*mode |= COL_ENABLED | COL_ENABLED_SET;

	return 0;
}

/* Set or unset other COL_* */
static int set_option(unsigned int *mode, unsigned int opt, int set)
{
	if (set)
		*mode |= opt;
	else
		*mode &= ~opt;
	return 0;
}

static int parse_option(const char *arg, int len,
			unsigned int *mode, int stdout_is_tty)
{
	struct colopt opts[] = {
		{ ENABLE, "always",  1 },
		{ ENABLE, "never",   0 },
		{ ENABLE, "auto",   -1 },
	};
	int i, set, name_len;

	for (i = 0; i < ARRAY_SIZE(opts); i++) {
		if (opts[i].type == OPTION) {
			if (len > 2 && !strncmp(arg, "no", 2)) {
				arg += 2;
				len -= 2;
				set = 0;
			}
			else
				set = 1;
		}

		name_len = strlen(opts[i].name);
		if (len != name_len ||
		    strncmp(arg, opts[i].name, name_len))
			continue;

		switch (opts[i].type) {
		case ENABLE: return set_enable_bit(mode, opts[i].value,
						   stdout_is_tty);
		case MODE: return set_mode(mode, opts[i].value);
		case OPTION: return set_option(mode, opts[i].value, set);
		default: die("BUG: Unknown option type %d", opts[i].type);
		}
	}

	return error("unsupported style '%s'", arg);
}

int git_config_column(unsigned int *mode, const char *value,
		      int stdout_is_tty)
{
	const char *sep = " ,";

	while (*value) {
		int len = strcspn(value, sep);
		if (len) {
			if (parse_option(value, len, mode, stdout_is_tty))
				return -1;

			value += len;
		}
		value += strspn(value, sep);
	}
	return 0;
}

int parseopt_column_callback(const struct option *opt,
			     const char *arg, int unset)
{
	unsigned int *mode = opt->value;
	if (unset) {
		*mode = (*mode & ~COL_ENABLED) | COL_ENABLED_SET;
		return 0;
	}
	if (arg)
		return git_config_column(mode, arg, -1);

	/* no arg, turn it on */
	*mode |= COL_ENABLED | COL_ENABLED_SET;
	return 0;
}
