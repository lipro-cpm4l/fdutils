

#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "driveprm.h"
#include "printfdprm.h"


struct floppy_struct floppy_type;
extern int errno;

void usage(char *name) __attribute__((noreturn));
void usage(char *name)
{
	fprintf(stderr,"Usage: %s\n", name);
	exit(1);
}

static void myprintf(char *fmt, int value)
{
  printf(fmt, value);
  putchar(' ');
}

int main(int argc,char **argv)
{
	int fd;
	char *name;
	level_t level;
	drivedesc_t drivedesc;

	level = LEV_NONE;
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
			case 'o':
				level = LEV_OLD;
				break;
			default:
				usage(argv[0]);
		}
		argc--;
		argv++;
	}	

	if ( argc == 1 )
		name = "/dev/fd0" ;
	else
		name = argv[1];

	fd=open(name,3);
	if ( fd < 0 )
		fd=open(name,O_RDONLY);
	if ( fd < 0 ){
		perror("open");
		exit(1);
	}
	
	if(ioctl(fd,FDGETPRM,&floppy_type)<0){
		perror("get geometry parameters");
		exit(1);
	}

	if(level == LEV_OLD) {
		printf("%4d %2d %d %2d %d 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x\n",
		       floppy_type.size,
		       floppy_type.sect,
		       floppy_type.head,
		       floppy_type.track,
		       floppy_type.stretch,
		       floppy_type.gap,
		       floppy_type.rate,
		       floppy_type.spec1,
		       floppy_type.fmt_gap);
	} else {
		parse_driveprm(fd, &drivedesc);
		print_params(&drivedesc, &floppy_type, level, 0, 
			     (void(*)(char *,int))myprintf);
	}
	close(fd);
	exit(0);
}
