#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fd.h>
#include <linux/fdreg.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/major.h>
#include "enh_options.h"


int eioctl(int fd, int command,void * param, char *emsg)
{
  int r;
  if ((r=ioctl(fd,command,param))<0 )
    {
      perror(emsg);
      exit(1);
    }
  return r;
}

static char procFd[80];


int doPoll(int fd,
	   struct floppy_drive_params *dpr,
	   struct floppy_drive_struct *state)
{
	if(! (dpr->flags & FD_SILENT_DCL_CLEAR)) {
		/* Work around a bug in floppy driver when silent dcl is not
		   set */
		struct floppy_raw_cmd raw_cmd;
		int fd2;
		fd2=open(procFd, 3 | O_NDELAY);
		if(fd2 != -1)
			close(fd2);
		/* Perform "dummy" rawcmd to flush out newchange */
		raw_cmd.flags = FD_RAW_NEED_DISK;
		raw_cmd.cmd_count = 0;
		raw_cmd.track = 0;
		ioctl(fd, FDRAWCMD, &raw_cmd, "rawcmd");
	}
	eioctl(fd, FDPOLLDRVSTAT, state, "reset");
	return state->flags;
}

int main(int argc, char **argv)
{
	int mask=0;
	int fd=-2;
	char *command=0;
	int interval=10;
	int ch;
	struct timeval timval;
	struct floppy_drive_struct state;
	struct floppy_drive_params dpr;

	struct enh_options optable[] = {
	{ 'd', "drive", 1, EO_TYPE_FILE_OR_FD, 3, 0,
		  (void *) &fd,
		  "drive to be polled"},
		
	{ 'e', "exec", 1, EO_TYPE_STRING, 0, 0,
		  (void *) &command,
		  "shell command to be executed after disk insertion" },
		
	{ 'i', "interval", 1, EO_TYPE_LONG, 0, 0, 
		  (void *) &interval,
		  "set polling interval (in tenth of seconds)" },
	{ '\0', 0 }		
	};

	while((ch=getopt_enh(argc, argv, optable, 
			     0, &mask, "drive") ) != EOF ){
		if ( ch== '?' ){
			fprintf(stderr,"exiting\n");
			exit(1);
		}
		printf("unhandled option %d\n", ch);
		exit(1);
	}

	if ( fd == -2 )
		fd = open("/dev/fd0", 3 | O_NDELAY);
	if ( fd < 0 ){
		perror("can't open floppy drive");
		exit(1);
	}

	eioctl(fd,FDGETDRVPRM,(void *) &dpr, "Get drive parameters");
	sprintf(procFd, "/proc/self/fd/%d", fd);

	doPoll(fd, &dpr, &state);
	while (state.flags & (FD_VERIFY|FD_DISK_NEWCHANGE)) {
		timval.tv_sec = interval / 10;
		timval.tv_usec = (interval % 10) * 100000;
		select(0, 0, 0, 0, &timval);
		doPoll(fd, &dpr, &state);
	} 
	close(fd);
	if ( command)
		system(command);
	exit(0);
}
