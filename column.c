#include "cache.h"
#include "column.h"
#include "string-list.h"
#include "parse-options.h"
#include "utf8.h"

#define MODE(mode) ((mode) & COL_MODE)
#define XY2LINEAR(d,x,y) (MODE((d)->mode) == COL_MODE_COLUMN ? \
			  (x) * (d)->rows + (y) : \
			  (y) * (d)->cols + (x))

struct column_data {
	const struct string_list *list; /* list of all cells */
	int mode;			/* COL_MODE */
	int total_width;		/* terminal width */
	int padding;			/* cell padding */
	const char *indent;		/* left most column indentation */
	const char *nl;

	int rows, cols;
	int *len;			/* cell length */
	int *width;			/* index to the longest row in column */
};

/* return length of 's' in letters, ANSI escapes stripped */
static int item_length(int mode, const char *s)
{
	int len, i = 0;
	struct strbuf str = STRBUF_INIT;

	if (!(mode & COL_ANSI))
		return utf8_strwidth(s);

	strbuf_addstr(&str, s);
	while ((s = strstr(str.buf + i, "\033[")) != NULL) {
		int len = strspn(s + 2, "0123456789;");
		i = s - str.buf;
		strbuf_remove(&str, i, len + 3); /* \033[<len><func char> */
	}
	len = utf8_strwidth(str.buf);
	strbuf_release(&str);
	return len;
}

/*
 * Calculate cell width, rows and cols for a table of equal cells, given
 * table width and how many spaces between cells.
 */
static void layout(struct column_data *data, int *width)
{
	int i;

	*width = 0;
	for (i = 0; i < data->list->nr; i++)
		if (*width < data->len[i])
			*width = data->len[i];

	*width += data->padding;

	data->cols = (data->total_width - strlen(data->indent)) / *width;
	if (data->cols == 0)
		data->cols = 1;

	data->rows = DIV_ROUND_UP(data->list->nr, data->cols);
}

static void compute_column_width(struct column_data *data)
{
	int i, x, y;
	for (x = 0; x < data->cols; x++) {
		data->width[x] = XY2LINEAR(data, x, 0);
		for (y = 0; y < data->rows; y++) {
			i = XY2LINEAR(data, x, y);
			if (i >= data->list->nr)
				continue;
			if (data->len[data->width[x]] < data->len[i])
				data->width[x] = i;
		}
	}
}

/*
 * Shrink all columns by shortening them one row each time (and adding
 * more columns along the way). Hopefully the longest cell will be
 * moved to the next column, column is shrunk so we have more space
 * for new columns. The process ends when the whole thing no longer
 * fits in data->total_width.
 */
static void shrink_columns(struct column_data *data)
{
	int x, y, total_width, cols, rows;

	data->width = xrealloc(data->width,
			       sizeof(*data->width) * data->cols);
	for (x = 0; x < data->cols; x++) {
		data->width[x] = 0;
		for (y = 0; y < data->rows; y++) {
			int len1 = data->len[data->width[x]];
			int len2 = data->len[XY2LINEAR(data, x, y)];
			if (len1 < len2)
				data->width[x] = y;
		}
	}

	while (data->rows > 1) {
		rows = data->rows;
		cols = data->cols;

		data->rows--;
		data->cols = DIV_ROUND_UP(data->list->nr, data->rows);
		if (data->cols != cols)
			data->width = xrealloc(data->width, sizeof(*data->width) * data->cols);

		compute_column_width(data);

		total_width = strlen(data->indent);
		for (x = 0; x < data->cols; x++) {
			total_width += data->len[data->width[x]];
			total_width += data->padding;
		}
		if (total_width > data->total_width) {
			data->rows = rows;
			data->cols = cols;
			compute_column_width(data);
			break;
		}
	}
}

/* Display without layout when COL_ENABLED is not set */
static void display_plain(const struct string_list *list,
			  const char *indent, const char *nl)
{
	int i;

	for (i = 0; i < list->nr; i++)
		printf("%s%s%s", indent, list->items[i].string, nl);
}

/* Print a cell to stdout with all necessary leading/traling space */
static int display_cell(struct column_data *data, int initial_width,
			const char *empty_cell, int x, int y)
{
	int i, len, newline;

	i = XY2LINEAR(data, x, y);
	if (i >= data->list->nr)
		return -1;

	len = data->len[i];
	if (data->width && data->len[data->width[x]] < initial_width) {
		/*
		 * empty_cell has initial_width chars, if real column
		 * is narrower, increase len a bit so we fill less
		 * space.
		 */
		len += initial_width - data->len[data->width[x]];
		len -= data->padding;
	}

	if (MODE(data->mode) == COL_MODE_COLUMN)
		newline = i + data->rows >= data->list->nr;
	else
		newline = x == data->cols - 1 || i == data->list->nr - 1;

	printf("%s%s%s",
			x == 0 ? data->indent : "",
			data->list->items[i].string,
			newline ? data->nl : empty_cell + len);
	return 0;
}

/* Display COL_MODE_COLUMN or COL_MODE_ROW */
static void display_table(const struct string_list *list,
			  int mode, int total_width,
			  int padding, const char *indent,
			  const char *nl)
{
	struct column_data data;
	int x, y, i, initial_width;
	char *empty_cell;

	memset(&data, 0, sizeof(data));
	data.list = list;
	data.mode = mode;
	data.total_width = total_width;
	data.padding = padding;
	data.indent = indent;
	data.nl = nl;

	data.len = xmalloc(sizeof(*data.len) * list->nr);
	for (i = 0; i < list->nr; i++)
		data.len[i] = item_length(mode, list->items[i].string);

	layout(&data, &initial_width);

	if (mode & COL_DENSE)
		shrink_columns(&data);

	empty_cell = xmalloc(initial_width + 1);
	memset(empty_cell, ' ', initial_width);
	empty_cell[initial_width] = '\0';
	for (y = 0; y < data.rows; y++) {
		for (x = 0; x < data.cols; x++)
			if (display_cell(&data, initial_width, empty_cell, x, y))
				break;
	}

	free(data.len);
	free(data.width);
	free(empty_cell);
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

	switch (MODE(mode)) {
	case COL_MODE_ROW:
	case COL_MODE_COLUMN:
		display_table(list, mode, width, padding, indent, nl);
		break;

	default:
		die("BUG: invalid mode %d", MODE(mode));
	}
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
		{ MODE,   "column", COL_MODE_COLUMN },
		{ MODE,   "row",    COL_MODE_ROW },
		{ OPTION, "color",  COL_ANSI },
		{ OPTION, "dense",  COL_DENSE },
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
