int scan_fdprm(FILE *f, char *name, struct floppy_struct *ft,
	       void (*callback)(char *, char *, struct floppy_struct *));
int parse_fdprm(int argc, char **argv, struct floppy_struct *ft);
