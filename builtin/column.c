#include "builtin.h"
#include "cache.h"
#include "strbuf.h"
#include "parse-options.h"
#include "string-list.h"
#include "column.h"

static const char * const builtin_column_usage[] = {
	"git column [options]",
	NULL
};
static unsigned int colopts;

int cmd_column(int argc, const char **argv, const char *prefix)
{
	struct string_list list = STRING_LIST_INIT_DUP;
	struct strbuf sb = STRBUF_INIT;
	struct column_options copts;
	struct option options[] = {
		OPT_COLUMN(0, "mode", &colopts, "layout to use"),
		OPT_INTEGER(0, "rawmode", &colopts, "layout to use"),
		OPT_INTEGER(0, "width", &copts.width, "Maximum width"),
		OPT_STRING(0, "indent", &copts.indent, "string", "Padding space on left border"),
		OPT_INTEGER(0, "nl", &copts.nl, "Padding space on right border"),
		OPT_INTEGER(0, "padding", &copts.padding, "Padding space between columns"),
		OPT_END()
	};

	memset(&copts, 0, sizeof(copts));
	copts.width = term_columns();
	copts.padding = 1;
	argc = parse_options(argc, argv, "", options, builtin_column_usage, 0);
	if (argc)
		usage_with_options(builtin_column_usage, options);

	while (!strbuf_getline(&sb, stdin, '\n'))
		string_list_append(&list, sb.buf);

	print_columns(&list, colopts, &copts);
	return 0;
}
