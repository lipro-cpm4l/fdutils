/*
 * Software patents declared unconstitutional, worldwide
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fd.h>
#include <linux/fdreg.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <sys/time.h>

#ifdef FD_RAW_MORE

char *error_msg[22]={
"Missing Data Address Mark",
"Bad cylinder",
"Scan not satisfied",
"Scan equal hit",
"Wrong cylinder",
"CRC error in data field",
"Control Mark = deleted",
0,

"Missing Address Mark",
"Write Protect",
"No Data - unreadable",
0,
"Overrun",
"CRC error in data or address",
0,
"End Of Cylinder",

0,
0,
0,
"Not ready",
"Equipment check error",
"Seek end" };

enum dir { READ_, WRITE_ } ;

int send_cmd(int fd,struct floppy_raw_cmd *raw_cmd, char *message)
{
  int i,j;
  int code;
  static int curtrack = -1;

  for (j=0; j<4; j++){
    if ( raw_cmd->track == curtrack && 
	!(raw_cmd->flags & FD_RAW_WRITE))
      raw_cmd->flags &= ~FD_RAW_NEED_SEEK;
    if ( ioctl( fd, FDRAWCMD, raw_cmd) < 0 ){
      curtrack = -1;
      if ( errno == EBUSY ){
	i--;
	fprintf(stderr,"\nFDC busy, sleeping for a second\n");
	sleep(1);
	continue;
      }
      if ( errno == EIO ){
	fprintf(stderr,"\nresetting controller\n");
	if(ioctl(fd, FDRESET, 2)  < 0){
	  perror("reset");
	  exit(1);
	}
	continue;
      }
      perror(message);
      exit(1);
    }

    if ( raw_cmd->reply_count ){
      switch( raw_cmd->reply[0] & 0xc0 ){
      case 0x40:
	curtrack = -1;
	fprintf(stderr,"\nerror during command execution\n");
	if ( raw_cmd->reply[1] & ST1_WP ){
	  fprintf(stderr,"The disk is write protected\n");
	  exit(1);
	}
	fprintf(stderr,"   ");
	for (i=0; i< raw_cmd->cmd_count; i++)
	  fprintf(stderr,"%2.2x ", (int)raw_cmd->cmd[i] );
	printf("\n");
	for (i=0; i< raw_cmd->reply_count; i++)
	  fprintf(stderr,"%2.2x ", (int)raw_cmd->reply[i] );
	fprintf(stderr,"\n");
	code = (raw_cmd->reply[0] <<16) + 
	  (raw_cmd->reply[1] << 8) + 
	    raw_cmd->reply[2];
	for(i=0; i<22; i++){
	  if ( (code & ( 1 << i )) && error_msg[i])
	    fprintf(stderr,"%s\n", error_msg[i]);
	}
	sleep(4);
	continue;
      case 0x80:
	curtrack = -1;
	fprintf(stderr,"\ninvalid command given\n");
	continue;
      case 0xc0:
	curtrack = -1;
	fprintf(stderr,"\nabnormal termination caused by polling\n");
	continue;
      case 0:
	if (raw_cmd->flags & FD_RAW_NEED_SEEK)
	  curtrack = raw_cmd->track;
	/* OK */
	return 0;
      }
    } else 
      fprintf(stderr,"\nNull reply from FDC\n");
  }
  fprintf(stderr,"\nToo many errors, giving up\n");
  exit(1);
}

int readwrite_sectors(int fd, /* file descriptor */
		      int drive,
		      enum dir direction,
		      int track, int head, int sector, int size, /* address */
		      char *data, 
		      int bytes,
		      struct floppy_raw_cmd *raw_cmd)
{
    raw_cmd->data = data;
    raw_cmd->length = bytes;
    
    raw_cmd->rate = 0;
    raw_cmd->flags = FD_RAW_INTR | FD_RAW_NEED_SEEK | 
	FD_RAW_NEED_DISK | FD_RAW_MORE;

    raw_cmd->cmd_count = 9;  
    
    if (direction == READ_) {
	raw_cmd->cmd[0] = FD_READ;
	raw_cmd->flags |= FD_RAW_READ;
    } else {
	raw_cmd->cmd[0] = FD_WRITE;
	raw_cmd->flags |= FD_RAW_WRITE;
    }

    raw_cmd->cmd[1] = (drive & 3) | (head << 2);
    
    raw_cmd->cmd[2] = track;
    raw_cmd->cmd[3] = head;
    raw_cmd->cmd[4] = sector;
    raw_cmd->cmd[5] = size;
    
    raw_cmd->cmd[6] = 0;
    raw_cmd->cmd[7] = 0x1b;
    raw_cmd->cmd[8] = 0xff;

    raw_cmd->track = track;
    return 0;
}

typedef struct sector_map {
  int head;
  int sector;
  int size;
  int bytes;
  int phantom;
} sector_map;

sector_map generic_map[]={
  { 0, 131, 3,1024, 0 },
  { 0, 132, 4,2048, 0 },
  { 1, 134, 6,8192, 0 },
  { 0, 130, 2, 512, 0 },
  { 1, 130, 2, 512, 0 },
  { 0, 134, 6,8192, 0 },
  { 1, 132, 4,2048, 0 },
  { 1, 131, 3,1024, 0 },
  { 0,   0, 0,   0, 0 }
};

sector_map zero_map[]={
  { 0, 129, 2, 11*512, 0 },
  { 1, 129, 2,  1*512, 0 },
  { 0,   1, 2,  8*512, 0 },
  { 0,   0, 0,  3*512, 1 },
  { 1, 130, 2, 14*512, 0 },
  { 0,   4, 2,  5*512, 2 },
  { 1, 144, 2,  4*512, 0 },
  { 0,   0, 0,      0, 0 }
};


#define POS(x) ( (skew + x) % 21 )

static int hs=0;
static int ts=14;
static int debug=0;

void format_track(int fd, int drive, int track, int head)
{
  int i;
  int skew;

  struct floppy_raw_cmd raw_cmd;

  struct format_map {
    unsigned char track;
    unsigned char head;
    unsigned char sector;
    unsigned char size;
  } format_map[21];

  for(i=0; i<21; i++){
    format_map[i].sector = 0;
    format_map[i].size = 0;
    format_map[i].track = track;
    format_map[i].head = head;
  }

  if ( track == 0 ){
    for (i=0; i<19; i++){
      if ( head )
	format_map[i].sector = i+129;
      else if ( i < 8 )
	format_map[i].sector = i+1;
      else
	format_map[i].sector = i-8+129;
      format_map[i].size = 2;
    }
    raw_cmd.cmd[3] = 19;
    raw_cmd.cmd[4] = 80;
  } else {
    skew = track * ts + head * hs;
    if (head){
      format_map[POS(1)].sector = 0x84;
      format_map[POS(1)].size = 0x4;
      format_map[POS(5)].sector = 0x82;
      format_map[POS(5)].size = 0x2;
      format_map[POS(6)].sector = 0x83;
      format_map[POS(6)].size = 0x3;
      format_map[POS(8)].sector = 0x86;
      format_map[POS(8)].size = 0x6;
    } else {
      format_map[POS(0)].sector = 0x83;
      format_map[POS(0)].size = 0x3;
      format_map[POS(2)].sector = 0x82;
      format_map[POS(2)].size = 0x2;
      format_map[POS(3)].sector = 0x84;
      format_map[POS(3)].size = 0x4;
      format_map[POS(7)].sector = 0x86;
      format_map[POS(7)].size = 0x6;
    }
    raw_cmd.cmd[3] = 21;
    raw_cmd.cmd[4] = 18;
  }

  raw_cmd.data = format_map;
  raw_cmd.length = sizeof(format_map[0]) * raw_cmd.cmd[3];
  
  raw_cmd.rate = 0;
  raw_cmd.flags = FD_RAW_INTR | FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK |
    FD_RAW_WRITE;
  
  raw_cmd.cmd[0] = FD_FORMAT;
  raw_cmd.cmd[1] = (drive & 3 ) | ( head << 2 );
  raw_cmd.cmd[2] = 2;  
  raw_cmd.cmd[5] = 42;
  raw_cmd.cmd_count = 6;  
  raw_cmd.track = track;
  send_cmd(fd, &raw_cmd, "format");
}

void readwrite_using_map(int fd, int drive, enum dir direction, 
			int track, char *data, sector_map *map)
{
    struct floppy_raw_cmd raw_cmd[20];
    int i,j;
    struct timeval tv1, tv2;

    j=i=0;
    while(map->bytes){
	if (map->phantom == 1 && direction == READ_)
	    memset(data, 0, map->bytes);
	else if (!map->phantom || direction == READ_) {
	    readwrite_sectors(fd, drive, direction,
			      track, map->head, map->sector, map->size,
			      data, map->bytes, raw_cmd + j);
	    j++;
	}
	data += map->bytes;
	map++;
	i++;
    }
    raw_cmd[j-1].flags &= ~ FD_RAW_MORE;
    if(debug)
	gettimeofday(&tv1,0);
    send_cmd(fd, raw_cmd, "read/write");
    if(debug) {
	gettimeofday(&tv2,0);
	printf("%d\n",
	       (tv2.tv_sec - tv1.tv_sec) * 1000000 + tv2.tv_usec - tv1.tv_usec);
    }
}

void readwrite_track(int fd, int drive, enum dir direction, 
		     int track, char *data)
{
  if ( track == 0 )
    readwrite_using_map(fd, drive, direction, 0, data, zero_map);
  else 
    readwrite_using_map(fd, drive, direction, track, data, generic_map);
}

static int get_type(int fd)
{
 int drive;
 struct stat statbuf;

 if (fstat(fd, &statbuf) < 0 ){
   perror("stat");
   exit(0);
 }
 
 if (!S_ISBLK(statbuf.st_mode) && MAJOR(statbuf.st_rdev) != FLOPPY_MAJOR)
   return -1;

 drive = MINOR( statbuf.st_rdev );
 return (drive & 3) + ((drive & 0x80) >> 5);
}

#define TRACKSIZE 512*2*23

static void usage(char *progname)
{
    fprintf(stderr,"Usage:\n");
    fprintf(stderr," For copying: %s [-n] <source> <target>\n", progname);
    fprintf(stderr,
	    " For formatting: %s [-D dosdrive] [-t trackskew] [-h headskew] <target>\n",
	    progname);
    exit(1);
}

static char *readme=
"This is an Xdf disk. To read it in Linux, you have to use a version of\r\n"
"mtools which is more recent than 2.5.4, and set the environmental\r\n"
"variable MTOOLS_USE_XDF before accessing it.\r\n\r\n"
"Bourne shell syntax (sh, ash, bash, ksh, zsh etc):\r\n"
" export MTOOLS_USE_XDF=1\r\n\r\n"
"C shell syntax (csh and tcsh):\r\n"
" setenv MTOOLS_USE_XDF 1\r\n\r\n"
"mtools can be gotten from ftp://ftp.imag.fr/pub/Linux/ZLIBC/mtools\r\n"
"\032";

int progress;

static void clear(void)
{
    if(progress)
	fprintf(stderr,"\b\b\b");
}


int main(int argc, char **argv)
{
    int sfd=0, tfd;
    int sdrive=0, tdrive;
    int track, head, ret;
    int c;
    int noformat=0;
    char dosdrive=0;

    char buffer[TRACKSIZE];
    char cmdbuffer[80];
    char *sourcename, *targetname;
    FILE *fp;
    
    while ((c = getopt(argc, argv, "t:h:dnD:")) != EOF) {
	switch (c) {
	    case 'D':
		dosdrive = optarg[0];
		break;
	    case 't':
		ts = atoi(optarg);
		break;
	    case 'h':
		hs = atoi(optarg);
		break;
	    case 'd':
		debug = 1;
	    break;
	    case 'n':
		noformat = 1;
		break;
	    default:
		usage(argv[0]);
	}
    }

    if(argc < optind +1 || argc >optind+2)
	usage(argv[0]);

    if(argc >= optind + 2)
	sourcename = argv[optind++];
    else
	sourcename = 0;
    targetname = argv[optind];

    if(!sourcename && noformat) {
	fprintf(stderr,
		"Missing sourcename\n");
	usage(argv[0]);
    }

    if(sourcename) {
	sfd = open( sourcename, O_RDONLY | O_NDELAY);
	if ( sfd < 0 ){
	    perror("Couldn't open source file");
	    exit(1);
	}
	
	sdrive = get_type(sfd);
    }

    tfd = open(targetname, O_WRONLY | O_CREAT | O_TRUNC | O_NDELAY, 0666);
    if ( tfd < 0 ){
	perror("Couldn't open target file");
	exit(1);
    }
    tdrive = get_type(tfd);
    if(tdrive < 0 && !sourcename) {
	fprintf(stderr, "Format target is not a floppy disk\n");
	usage(argv[0]);
    }
    
    progress = (tdrive >= 0 || sdrive >= 0) && isatty(2);

    for ( track = 0 ; track < 80 ; track++){
	if(sourcename) {
	    if ( sdrive == - 1 ){
		ret = read(sfd, buffer, TRACKSIZE );
		if ( ret < 0 ){
		    perror("read");
		    exit(1);
		}
		if ( ret < TRACKSIZE ){
		    fprintf(stderr,"short read\n");
		    exit(1);
		}
	    } else {
		if(progress) {
		    fprintf(stderr,"r%02d", track);
		    fflush(stderr);
		}
		readwrite_track(sfd, sdrive, READ, track, buffer);
		clear();
	    }
	} else
	    memset(buffer, 0, TRACKSIZE);
	
	if (tdrive == -1){
	    ret = write(tfd, buffer, TRACKSIZE);
	    if ( ret < 0 ){
		perror("write");
		exit(1);
	    }
	    if ( ret < TRACKSIZE ){
		fprintf(stderr,"short write\n");
		exit(1);
	    }
	} else {
	    if(!noformat) {
		if(progress) {
		    fprintf(stderr,"f%02d", track);
		    fflush(stderr);
		}
		for(head = 0; head < 2; head++)
		    format_track(tfd, tdrive, track,head);
		clear();
	    }
	    if(progress) {
		fprintf(stderr,"w%02d", track);
		fflush(stderr);
	    }
	    readwrite_track(tfd, tdrive, WRITE, track, buffer);
	    clear();
	}
    }
    close(tfd);
    close(sfd);

    if(dosdrive && !sourcename && !noformat) {
	sprintf(cmdbuffer,"mformat -X %c:", dosdrive);
	system(cmdbuffer);
	unsetenv("MTOOLS_USE_XDF");
	sprintf(cmdbuffer,"mformat -t1 -h1 -s11 %c:", dosdrive);
	system(cmdbuffer);
	sprintf(cmdbuffer,"mcopy - %c:README", dosdrive);
	fp = popen(cmdbuffer, "w");
	if(fp) {
	    fwrite(readme, strlen(readme), 1,  fp);
	    fclose(fp);
	}
    }
    exit(0);
}

#else
void main(void)
{
	fprintf(stderr,"xdfcopy not supported on this system\n");
	fprintf(stderr,"Please upgrade your kernel and recompile fdutils\n");
	exit(1);
}
#endif
