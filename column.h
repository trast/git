#ifndef COLUMN_H
#define COLUMN_H

#define COL_MODE          0x000F
#define COL_ENABLED      (1 << 4)

struct column_options {
	int width;
	int padding;
	const char *indent;
	const char *nl;
};

extern int term_columns(void);
extern void print_columns(const struct string_list *list,
			  unsigned int mode,
			  struct column_options *opts);

#endif
