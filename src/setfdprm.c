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
#include "enh_options.h"
#include "mediaprm.h"
#include "driveprm.h"
#include "oldfdprm.h"

int mcmd = 0;
int cmd = FDSETPRM;

struct enh_options optable[] = {
	{ 'c', "clear", 0, EO_TYPE_NONE, FDCLRPRM, 0, &cmd,
	  "clear the geometry parameters" },
	{ 'p', "permanent", 0, EO_TYPE_NONE, FDCLRPRM, 0, &cmd,
	  "set the geometry parameters permanently" },
	{ 'y', "message-on", 0, EO_TYPE_NONE, FDMSGON, 0, &mcmd,
	  "switch messages on" },
	{ 'n', "message-off", 0, EO_TYPE_NONE, FDMSGOFF, 0, &mcmd,
	  "switch messages off" },
	{ '\0', 0 }
};

int main(int argc,char **argv)
{
	int cmd,fd,c;
	char *name;
	drivedesc_t drivedesc;
	struct floppy_struct medprm;
	int have_fmt = 0;
	char *userparams="drive [geometry]";
	int mask;

	name = argv[0];
	if (argc < 3) 
		print_usage(name, optable, userparams);
	cmd = FDSETPRM;
	mcmd = 0;
	while((c=getopt_enh(argc, argv, optable, 0, &mask, userparams)) != EOF){
		if(c == '?') {
			fprintf(stderr,"exiting\n");
			exit(1);
		}
		printf("unhandled option %d\n", c);
		exit(1);
	}

	argv += optind;
	argc -= optind;

	if ((fd = open(argv[0],3)) < 0) { /* 3 == no access at all */
		perror(argv[0]);
		exit(1);
	}
	argv++;
	argc--;
	parse_driveprm(fd, &drivedesc);

	if(argc) {
		if(parse_mediaprm(argc, argv, &drivedesc, &medprm) &&
		   parse_fdprm(argc, argv, &medprm)) {
			print_usage(name, optable, userparams);
			exit(1);
		}
		have_fmt = 1;
	}

	if(mcmd && ioctl(fd, cmd) < 0) {
		perror("message ioctl");
		exit(1);
	}

	if(have_fmt || cmd == FDCLRPRM) {
		if(ioctl(fd, cmd, &medprm) < 0) {
			perror("geometry ioctl");
			exit(1);
		}
	}

	exit(0);
}
