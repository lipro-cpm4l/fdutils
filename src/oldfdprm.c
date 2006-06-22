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

#define FDPRMFILE "/etc/fdprm"
#define MAXLINE   200


static unsigned long convert(char *arg, int *error)
{
	unsigned long result;
	char *end;
	
	result = strtoul(arg,&end,0);
	if (!*end) 
		return result;
	*error = 1;
	return 0;
}

static int set_params(char **params, struct floppy_struct *ft)
{
	int error;

	error = 0;
	ft->size = convert(params[0],&error);
	ft->sect = convert(params[1],&error);
	ft->head = convert(params[2],&error);
	ft->track = convert(params[3],&error);
	ft->stretch = convert(params[4],&error);
	ft->gap = convert(params[5],&error);
	ft->rate = convert(params[6],&error);
	ft->spec1 = convert(params[7],&error);
	ft->fmt_gap = convert(params[8],&error);
	return error;
}

int scan_fdprm(FILE *file, char *name, struct floppy_struct *ft, 
	       void (*callback)(char *name, char *comment,
				struct floppy_struct *ft))
{
	char line[MAXLINE+2],this[MAXLINE+2],param[9][MAXLINE+2];
	char *params[9],*start;
	char *comment;
	int i, lineno;
	
	lineno = 0;
	while (fgets(line,MAXLINE,file)) {
		lineno++;
		for (start = line; *start == ' ' || *start == '\t'; start++);
		if (!*start || *start == '\n' || *start == '#') {
			/* comment line, print as is */
			if(!name)
				printf("%s", line);
			continue;
		}
		
		comment = strchr(start, '#');
		if(comment) {
			*comment='\0';
			comment++;
		}
		if (sscanf(start,"%s %s %s %s %s %s %s %s %s %s",this,param[0],
			   param[1],param[2],param[3],param[4],param[5],param[6],
			   param[7],param[8]) != 10) {
			fprintf(stderr,"Syntax error in line %d: '%s'\n",
				lineno, line);
			exit(1);
		}
		for(i=0;i<9;i++)
			params[i]=param[i];
		if(!name || !strcmp(this,name) ) {			
			i=set_params(params, ft);
			if(name)
				return 0;
			else
				callback(this, comment, ft);
		}
	}
	return 1;
}


int parse_fdprm(int argc, char **argv, struct floppy_struct *ft)
{
	FILE *f;
	int r;
	switch(argc) {
		case 1:
			/* indirect */
			if ((f = fopen(FDPRMFILE,"r")) == NULL)
				return 1;
			r = scan_fdprm(f, argv[0], ft,0);
			fclose(f);
			return r;
		case 9:
			return set_params(argv, ft);
		default:
			return 1;
	}
}
