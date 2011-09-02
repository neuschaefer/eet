/*
 * eet - backward tee
 *
 * Copyright (C) 2011  Jonathan Neuschäfer
 *
 * eet opens multiple files for reading and watches them for readability.
 * When a file becomes readable, it reads from it and writes the read data
 * on stdout.
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <signal.h>

/* this is GNU-only */
#include <error.h>

/* only for stderr */
#include <stdio.h>

#include <ev.h>

#include "eet.h"
#include "tty.h"

#define zalloc(x) calloc(1, x)

#define container_of(ptr,type,member) \
	((type *) (((char *) ptr) - ((char *) offsetof(type, member))))

static struct eet *last_eet;

static char read_buf[BUFSIZ];

static int xwrite(int fd, char *buf, size_t _sz)
{
	int ret, sz = _sz;

	while (sz) {
		ret = write(fd, buf, sz);
		if (ret == -1)
			return -1;
		buf += ret;
		sz -= ret;
	}
	return _sz;
}

static int open_eet(char *path)
{
	int fd;

	if (path[0] == '-' && !path[1]) {
		/* the special case of an stdin alias */
		fd = dup(STDIN_FILENO);
		if (fd == -1) {
			error(0, errno, "can't dup stdin!");
			return -1;
		}
	} else {
		/*
		 * no CLOEXEC here, we *won't* exec
		 */
		fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd == -1) {
			error(0, errno, "unable to open `%s' for reading", path);
			return -1;
		}
	}

	return fd;
}

static void handle_eof(EV_P_ struct eet *eet)
{
	int fd = eet->watcher.fd;

	if (eet->flags & EET_KEEPOPEN)
		return;

	tty_restore(eet->tty, fd);
	eet->tty = NULL;

	ev_io_stop(EV_A_ &eet->watcher);

	close(fd);

	if (!(eet->flags & EET_REOPEN)) {
		eet->flags |= EET_STOPPED;
		return;
	}

	/* reopen */
	fd = open_eet(eet->path);
	if (fd == -1) {
		eet->flags |= EET_STOPPED;
		return;
	}

	eet->tty = tty_set(fd);

	ev_io_set(&eet->watcher, fd, EV_READ);
	ev_io_start(EV_A_ &eet->watcher);
}

static void eet_read_cb(EV_P_ ev_io *w, int revents)
{
	struct eet *eet = container_of(w, struct eet, watcher);
	int ret;

	ret = read(w->fd, read_buf, sizeof(read_buf));
	if (ret == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;

		error(0, errno, "error reading from `%s'", eet->path);
		ev_io_stop(EV_A_ w);
		eet->flags |= EET_STOPPED;
		return;
	}

	if (!ret) {
		handle_eof(EV_A_ eet);
		return;
	}

	/*
	 * we assume that stdout does not block too much while being written to
	 */
	ret = xwrite(STDOUT_FILENO, read_buf, ret);
	if (ret == -1) {
		error(0, errno, "error writing to stdout; serious problems!");
		ev_break(EV_A_ EVBREAK_ALL);
	}
}

static void new_eet(char *path, unsigned flags)
{
	int fd;
	struct eet *new;

	fd = open_eet(path);
	if (fd == -1)
		return;

	new = zalloc(sizeof(*new));
	if (!new) {
		error(0, errno, "unable to set up `%s'", path);
		goto out_alloc;
	}

	new->path = path;
	new->flags = flags;

	if (!(flags & EET_NOTTY))
		new->tty = tty_set(fd);

	ev_io_init(&new->watcher, eet_read_cb, fd, EV_READ);
	ev_io_start(EV_DEFAULT_UC_ &new->watcher);

	new->prev = last_eet;
	last_eet = new;

	return;

out_alloc:
	close(fd);
}

static void parse_cmdline(int argc, char **argv)
{
	unsigned flags = 0;

	/* we certainly don't want to open argv[0] */
	argv++;

	for (; *argv; argv++) {
		if (**argv == '-' && (*argv)[1]) {
			/* it's an option */
			char *tmp = *argv;
			while (*++tmp) {
				switch (*tmp) {
				case 'k':
					flags |= EET_KEEPOPEN;
					break;
				case 'r':
					flags |= EET_REOPEN;
					break;
				case 't':
					flags |= EET_NOTTY;
					break;
				default:
					fprintf(stderr, "invalid option `-%c'\n", *tmp);
					exit(EXIT_FAILURE);
				}
			}
		} else {
			new_eet(*argv, flags);
			flags = 0;
		}
	}
}

/* it should only be called outside of an ev_loop */
static void close_all(void)
{
	struct eet *eet, *prev;
	int fd;

	if (!last_eet)
		return;

	for (eet = last_eet; eet; eet = prev) {
		prev = eet->prev;
		if (!(eet->flags & EET_STOPPED)) {
			fd = eet->watcher.fd;
			tty_restore(eet->tty, fd);
			close(fd);
		}
		free(eet);
	}
}

static void sigint_cb(EV_P_ ev_signal *w, int revents)
{
	ev_break(EV_A_ EVBREAK_ALL);
}

static ev_signal sigint_w;

int main(int argc, char **argv)
{
	if (!ev_default_loop(0)) {
		fprintf(stderr, "%s: cannot initalise the libev mainloop\n",
				argv[0]);
		return EXIT_FAILURE;
	}

	parse_cmdline(argc, argv);

	ev_signal_init(&sigint_w, sigint_cb, SIGINT);
	ev_signal_start(EV_DEFAULT_UC_ &sigint_w);
	ev_unref(EV_DEFAULT_UC);

	ev_run(EV_DEFAULT_UC_ 0);

	close_all();

	return EXIT_SUCCESS;
}
