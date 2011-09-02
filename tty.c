/*
 * tty.c - quite generic tty setting/unsetting
 */

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

#include "tty.h"

struct tty_info {
	struct termios tios;
};

static struct termios tios_buf;

struct tty_info *tty_set(int fd)
{
	struct tty_info *tty;

	if (!isatty(fd))
		return NULL;

	tty = malloc(sizeof(*tty));
	if (!tty)
		return NULL;

	if (tcgetattr(fd, &tty->tios) == -1)
		goto out_free;

	/* gcc can copy efficiently, I hope */
	tios_buf = tty->tios;

	tios_buf.c_lflag &= ~(ICANON|ECHO);

	if (tcsetattr(fd, TCSANOW, &tios_buf) == -1)
		goto out_free;

	return tty;

out_free:
	free(tty);
	return NULL;
}

void tty_restore(struct tty_info *tty, int fd)
{
	if (!tty)
		return;

	/* errors are ingnored here */
	tcsetattr(fd, TCSANOW, &tty->tios);

	free(tty);
}
