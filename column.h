#ifndef COLUMN_H
#define COLUMN_H

#define COL_MODE          0x000F
#define COL_MODE_COLUMN        0   /* Fill columns before rows */
#define COL_MODE_ROW           1   /* Fill rows before columns */
#define COL_ENABLED      (1 << 4)
#define COL_ENABLED_SET  (1 << 5)  /* Has COL_ENABLED been set by config? */
#define COL_ANSI         (1 << 6)  /* Remove ANSI escapes from string length */
#define COL_DENSE        (1 << 7)  /* Shrink columns when possible,
				      making space for more columns */

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
extern int git_config_column(unsigned int *mode, const char *value,
			     int stdout_is_tty);
extern int git_column_config(const char *var, const char *value,
			     const char *command, unsigned int *colopts);

struct option;
extern int parseopt_column_callback(const struct option *opt,
				    const char *arg, int unset);

extern int run_column_filter(int colopts, const struct column_options *);
extern int stop_column_filter();

#endif
