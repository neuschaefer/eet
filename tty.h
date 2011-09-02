struct tty_info;

extern struct tty_info *tty_set(int);
extern void tty_restore(struct tty_info *, int);
