/*
 * eet - backward tee
 */

#include <ev.h>

#define EET_KEEPOPEN	(1 << 0) /* don't close on eof */
#define EET_REOPEN	(1 << 1) /* reopen on eof */
#define EET_STOPPED	(1 << 2) /* watcher stopped, fd closed */
#define EET_NOTTY	(1 << 3) /* don't treat this as a tty */

/* as defined in tty.h */
struct tty_info;

/* this struct represents one file being watched */
struct eet {
	/*
	 * path. points to a cmdline arg (stack)
	 */
	char *path;

	unsigned flags;
	ev_io watcher;
	struct tty_info *tty;

	struct eet *prev;
};
