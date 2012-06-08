#ifndef LINE_H
#define LINE_H

#include "diffcore.h"

/*
 * Parse one item in an -L begin,end option w.r.t. the notional file
 * object 'cb_data' consisting of 'lines' lines.
 *
 * The 'nth_line_cb' callback is used to determine the start of the
 * line 'lno' inside the 'cb_data'.  The caller is expected to already
 * have a suitable map at hand to make this a constant-time lookup.
 *
 * Returns 0 in case of success and -1 if there was an error.  The
 * caller should print a usage message in the latter case.
 */

typedef const char *(*nth_line_fn_t)(void *data, long lno);

extern int parse_range_arg(const char *arg,
			   nth_line_fn_t nth_line_cb,
			   void *cb_data, long lines,
			   long *begin, long *end);

/*
 * Scan past a range argument that could be parsed by
 * 'parse_range_arg', to help the caller determine the start of the
 * filename in '-L n,m:file' syntax.
 *
 * Returns a pointer to the first character after the 'n,m' part, or
 * NULL in case the argument is obviously malformed.
 */

extern const char *skip_range_arg(const char *arg);

struct rev_info;
struct commit;

/* A range [start,end].  Lines are numbered starting at 0, and the
 * ranges include start but exclude end. */
struct range {
	long start, end;
};

/* A set of ranges.  The ranges must always be disjoint and sorted. */
struct range_set {
	int alloc, nr;
	struct range *ranges;
};

/* A diff, encoded as the set of pre- and post-image ranges where the
 * files differ. A pair of ranges corresponds to a hunk. */
struct diff_ranges {
	struct range_set parent;
	struct range_set target;
};

/* Linked list of interesting files and their associated ranges.  The
 * list must be kept sorted by spec->path */
struct line_log_data {
	struct line_log_data *next;
	struct diff_filespec *spec;
	char status;
	struct range_set ranges;
	int arg_alloc, arg_nr;
	char **args;
	struct diff_filepair *pair;
	struct diff_ranges diff;
};

extern void line_log_data_init(struct line_log_data *r);

extern void line_log_init(struct rev_info *rev, struct line_log_data *r);

extern int line_log_filter(struct rev_info *rev);

extern int line_log_print(struct rev_info *rev, struct commit *commit);

#endif /* LINE_H */
