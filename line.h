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
struct diff_line_range;
struct diff_options;

struct line_range;

struct diff_line_range {
	struct diff_filespec *prev;
	struct diff_filespec *spec;
	char status;
	int alloc;
	int nr;
	struct line_range *ranges;
	unsigned int	touched:1,
			diff:1;
	struct diff_line_range *next;
};

static inline void diff_line_range_init(struct diff_line_range *r)
{
	r->prev = r->spec = NULL;
	r->status = '\0';
	r->alloc = r->nr = 0;
	r->ranges = NULL;
	r->next = NULL;
	r->touched = 0;
	r->diff = 0;
}

extern void diff_line_range_append(struct diff_line_range *r, const char *arg);

extern void line_log_init(struct rev_info *rev, struct diff_line_range *r);

extern int line_log_walk(struct rev_info *rev);

#endif /* LINE_H */
