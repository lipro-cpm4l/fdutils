
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fdreg.h>
#include <linux/fd.h>
#include <linux/errno.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

struct floppy_struct floppy_type;
extern int errno;

void main(int argc,char **argv)
{
  int fd;

  char *name;

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
    perror("");
    exit(1);
  }

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
  close(fd);
}







