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
#include <linux/fs.h>
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


void main(int argc, char **argv)
{
	int mask=0;
	int fd=-2;
	char *command=0;
	int interval=10;
	char ch;
	struct timeval timval;
	struct floppy_drive_struct state;

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
		print_usage(argv[0],optable,"");
	}

	eioctl(fd, FDPOLLDRVSTAT, &state, "reset");
	while (state.flags & FD_VERIFY) {
		timval.tv_sec = interval / 10;
		timval.tv_usec = (interval % 10) * 100000;
		select(0, 0, 0, 0, &timval);
		eioctl(fd, FDPOLLDRVSTAT, &state, "reset");
	} 
	close(fd);
	if ( command)
		system(command);
	exit(0);
}
