#include <sys/types.h>
#ifdef HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>
#endif
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
#include <sys/signal.h>
#include <errno.h>

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

#ifndef PIDFILE
#define PIDFILE NULL
#endif

void dummy(int dummy)
{}

int main(int argc, char **argv)
{
	int mask=0;
	int fd=-2;
	int interval=1000;
	int ch;
	struct timeval timval;
	struct floppy_raw_cmd raw_cmd;
	struct stat buf;
	int drive;
	char *drive_name="/dev/fd0";
	char *pidfilename=PIDFILE;
	char *tmp_s;
	FILE *pidfile;
	int pid;

	struct enh_options optable[] = {
	{ 'd', "drive", 1, EO_TYPE_STRING, 0, 0,
		  (void *) &drive_name,
		  "drive to be seeked"},

	{ 'i', "interval", 1, EO_TYPE_LONG, 0, 0, 
		  (void *) &interval,
		  "set polling interval (in tenth of seconds)" },

	{ 'p', "pidfile", 1, EO_TYPE_STRING, 0, 0,
		  (void *) &pidfilename,
		  "stores the daemon process id into named file"},

	{ '\0', 0 }
	};

	tmp_s = strrchr(argv[0],'/');
	if(!tmp_s)
		tmp_s = argv[0];
	else
		tmp_s++;
	if ( !strcmp(tmp_s, "diskseek" ))
		interval = 0;

	while((ch=getopt_enh(argc, argv, optable, 
			     0, &mask, "drive") ) != EOF ){
		if ( ch== '?' ){
			fprintf(stderr,"exiting\n");
			exit(1);
		}
		printf("unhandled option %d\n", ch);
		exit(1);
	}

	if(interval){
		switch((pid = fork())){
		case 0: /* child */
			break;
		case -1: /* error */
			perror("fork");
			exit(1);
		default: /* parent */
			if(!pidfilename)
				pidfilename = getenv("DISKSEEKDPIDFILE");
			if(pidfilename){				
				pidfile = fopen(pidfilename, "w");
				fprintf(pidfile, "%d\n", pid);
				fclose(pidfile);
			}
			exit(0);
		}
	}

	while(1){
		signal(SIGHUP, dummy);
		/* open the device */
		fd=open(drive_name, O_ACCMODE | O_EXCL);

		if (fd < 0 ){
			fprintf(stderr,"%s\n", drive_name);
			perror("open");
			exit(1);
		}

		if (fstat (fd, &buf) < 0) {
			perror("fstat");
			exit(1);
		}
		if (major(buf.st_rdev) != FLOPPY_MAJOR) {
			fprintf(stderr,"Not a floppy drive\n");
			exit(1);
		}

		drive = minor( buf.st_rdev );
		drive = (drive & 3) + ((drive & 0x80) >> 5);

		/* reset the fdc, if needed */
		eioctl(fd, FDRESET, FD_RESET_IF_NEEDED, "reset");

		/* then recalibrate the disk */
		raw_cmd.flags = FD_RAW_INTR;
		raw_cmd.cmd_count = 2;
		raw_cmd.cmd[0] = FD_RECALIBRATE;
		raw_cmd.cmd[1] = drive;
		eioctl(fd, FDRAWCMD, (void *)&raw_cmd, "recalibrate");

		raw_cmd.flags = FD_RAW_INTR;
		raw_cmd.cmd_count = 3;
		raw_cmd.cmd[0] = FD_SEEK;
		raw_cmd.cmd[1] = drive & 3;
		raw_cmd.cmd[2] = 80;
		eioctl(fd, FDRAWCMD, (void *)&raw_cmd, "blind seek");
			
		/* then recalibrate the disk */
		raw_cmd.flags = FD_RAW_INTR;
		raw_cmd.cmd_count = 2;
		raw_cmd.cmd[0] = FD_RECALIBRATE;
		raw_cmd.cmd[1] = drive;
		eioctl(fd, FDRAWCMD, (void *)&raw_cmd, "recalibrate");
		close(fd);

		if(!interval)
			exit(0);
		timval.tv_sec = interval;
		timval.tv_usec = 0;
		if (select(0, 0, 0, 0, &timval) < 0 && errno != EINTR){
			perror("select");
			exit(1);
		}
	}
    return 0;
}
