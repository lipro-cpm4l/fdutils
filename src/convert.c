/* setfdprm.c  -  Sets user-provided floppy disk parameters, re-activates
		  autodetection and switches diagnostic messages. */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fd.h>
#include "oldfdprm.h"
#include "printfdprm.h"

#define FDPRMFILE "/etc/fdprm"
#define MAXLINE   200

static int col = 0;

static int cpm=1;
level_t level = LEV_NONE;

static void print_token(char *fmt, int param)
{
	char buffer[50];
	int l;

	snprintf(buffer, 49, fmt, param);
	l = strlen(buffer);

	if(l + col > 70) {
		printf("\n");
		col = 1;
	}
	printf(" %s", buffer);
	col += l+1;
}


static void _print_params(char *name, char *comment, struct floppy_struct *ft)
{
	printf("\n\"%s\":", name);
	if(comment)
		printf(" #%s", comment);
	else
		printf("\n");
	col = 0;
	print_params(0, ft, level, cpm, print_token);
}

static void usage(char *name)
{
}


int main(int argc,char **argv)
{
	struct floppy_struct ft;
	char *name = argv[0];

	
	if (argc > 1 && *argv[1] == '-') {
		switch (argv[1][1]) {
			case 'e':
				level = LEV_EXPL;
				break;
			case 'm':
				level = LEV_MOST;
				break;
			case 'a':
				level = LEV_ALL;
				break;
			default:
				usage(name);
		}
		argc--;
		argv++;
	}	

	scan_fdprm(stdin, 0, &ft, _print_params);
	exit(0);
}
