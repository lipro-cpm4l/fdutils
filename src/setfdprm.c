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

#define FDPRMFILE "/etc/fdprm"
#define MAXLINE   200


static int convert(char *arg)
{
    long result;
    char *end;

    result = strtol(arg,&end,0);
    if (!*end) return (int) result;
    fprintf(stderr,"Invalid number: %s\n",arg);
    exit(1);
}


static void cmd_without_param(int cmd,int fd)
{
    if (ioctl(fd,cmd,NULL) >= 0) exit(0);
    perror("ioctl");
    exit(1);
}


static void set_params(int cmd,int fd,char **params)
{
    struct floppy_struct ft;

    ft.size = convert(params[0]);
    ft.sect = convert(params[1]);
    ft.head = convert(params[2]);
    ft.track = convert(params[3]);
    ft.stretch = convert(params[4]);
    ft.gap = convert(params[5]);
    ft.rate = convert(params[6]);
    ft.spec1 = convert(params[7]);
    ft.fmt_gap = convert(params[8]);
    ft.name = NULL;
    if (ioctl(fd,cmd,&ft) >= 0) exit(0);
    perror("ioctl");
    exit(1);
}


static void find_params(int cmd,int fd,char *name)
{
    FILE *file;
    char line[MAXLINE+2],this[MAXLINE+2],param[9][MAXLINE+2];
    char *params[9],*start;
    int count;

    if ((file = fopen(FDPRMFILE,"r")) == NULL) {
	perror(FDPRMFILE);
	exit(1);
    }
    while (fgets(line,MAXLINE,file)) {
	for (start = line; *start == ' ' || *start == '\t'; start++);
	if (*start && *start != '\n' && *start != '#') {
	    if (sscanf(start,"%s %s %s %s %s %s %s %s %s %s",this,param[0],
	      param[1],param[2],param[3],param[4],param[5],param[6],param[7],
	      param[8]) != 10) {
		fprintf(stderr,"Syntax error: '%s'\n",line);
		exit(1);
	    }
	    if (!strcmp(this,name)) {
		for (count = 0; count < 9; count++)
		    params[count] = param[count];
		set_params(cmd,fd,params);
	    }
	}
    }
    fprintf(stderr,"No such parameter set: '%s'\n",name);
    exit(1);
}


static void usage(char *name)
{
    char *this;

    if ((this = strrchr(name,'/'))) name = this+1;
    fprintf(stderr,"usage: %s [ -p ] dev name\n",name);
    fprintf(stderr,"       %s [ -p ] dev size sect heads tracks stretch \
gap rate spec1 fmt_gap\n",name);
#ifdef FDMEDCNG
    fprintf(stderr,"       %s [ -c | -y | -n | -d ] dev\n",name);
#else
    fprintf(stderr,"       %s [ -c | -y | -n ] dev\n",name);
#endif
    exit(1);
}


void main(int argc,char **argv)
{
    int cmd,fd;
    char *name;

    name = argv[0];
    if (argc < 3) usage(name);
    cmd = FDSETPRM;
    if (*argv[1] == '-') {
	switch (argv[1][1]) {
	    case 'c':
		cmd = FDCLRPRM;
		break;
	    case 'p':
		cmd = FDDEFPRM;
		break;
	    case 'y':
		cmd = FDMSGON;
		break;
	    case 'n':
		cmd = FDMSGOFF;
		break;
#ifdef FDMEDCNG
	    case 'd':
		cmd = FDMEDCNG;
		break;
#endif
	    default:
		usage(name);
	}
	argc--;
	argv++;
    }
    if ((fd = open(argv[1],3)) < 0) { /* 3 == no access at all */
	perror(argv[1]);
	exit(1);
    }
    if (cmd != FDSETPRM && cmd != FDDEFPRM) {
	if (argc != 2) usage(name);
	cmd_without_param(cmd,fd);
    }
    if (argc != 11 && argc != 3) usage(name);
    if (argc == 11) set_params(cmd,fd,&argv[2]);
    else find_params(cmd,fd,argv[2]);
}
