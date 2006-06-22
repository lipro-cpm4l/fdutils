/*
 * Software patents declared unconstitutional, worldwide
 */

#include <sys/types.h>
#ifdef HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>
#endif
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
#include <sys/time.h>
#include <errno.h>
#include "fdutils.h"

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

static int curcylinder = -1;

int interpret_errors(struct floppy_raw_cmd *raw_cmd, int probe_only)
{
	int i,k;
	int code;

	k = 0;
	do {
		if ( raw_cmd[k].reply_count ){
			switch( raw_cmd[k].reply[0] & 0xc0 ){
				case 0x40:
					if((raw_cmd[k].reply[0] & 0x38) == 0 &&
					   raw_cmd[k].reply[1] == 0x80 &&
					   raw_cmd[k].reply[2] == 0)
						break;

					if(probe_only)
						return -2;
					curcylinder = -1;
					fprintf(stderr,
						"\nerror during command execution\n");
					if ( raw_cmd[k].reply[1] & ST1_WP ){
						fprintf(stderr,
							"The disk is write protected\n");
						exit(1);
					}
					fprintf(stderr,"   ");
					for (i=0; i< raw_cmd[k].cmd_count; i++)
						fprintf(stderr,"%2.2x ", 
							(int)raw_cmd[k].cmd[i] );
					printf("\n");
					for (i=0; i< raw_cmd[k].reply_count; i++)
						fprintf(stderr,"%2.2x ", 
							(int)raw_cmd[k].reply[i] );
					fprintf(stderr,"\n");
					code = (raw_cmd[k].reply[0] <<16) + 
						(raw_cmd[k].reply[1] << 8) + 
						raw_cmd[k].reply[2];
					for(i=0; i<22; i++){
					if ( (code & ( 1 << i )) && 
					     error_msg[i])
						fprintf(stderr, "%s\n",
							error_msg[i]);
					}
					sleep(4);
					return k;
				case 0x80:
					curcylinder = -1;
					fprintf(stderr,
						"\ninvalid command given\n");
					return 1;
				case 0xc0:
					curcylinder = -1;
					fprintf(stderr,
						"\nabnormal termination caused by polling\n");
					return 0;
				case 0:
					break;
			}
			if (raw_cmd[k].flags & FD_RAW_NEED_SEEK)
			    curcylinder = raw_cmd[k].track;
			/* OK */			
		} else {
			fprintf(stderr,"\nNull reply from FDC\n");
			return 1;
		}
		k++;
	} while(raw_cmd[k-1].flags & FD_RAW_MORE);
	return -1;
}

int send_cmd(int fd,struct floppy_raw_cmd *raw_cmd, char *message, int probe_only)
{
	int i,j,k;
	
	for (j=0; j<4; j++){
		if ( raw_cmd->track == curcylinder && 
		     !(raw_cmd->flags & FD_RAW_WRITE))
			raw_cmd->flags &= ~FD_RAW_NEED_SEEK;
		if ( ioctl( fd, FDRAWCMD, raw_cmd) < 0 ){
			curcylinder = -1;
			if (errno == EBUSY){
				i--;
				fprintf(stderr,
					"\nFDC busy, sleeping for a second\n");
				sleep(1);
				continue;
			}
			if (errno == EIO){
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
		

		k = interpret_errors(raw_cmd, probe_only);
		switch(k) {
			case -2:
				return 1;
			case -1:
				return 0;
		}
		raw_cmd += k;
	}
	fprintf(stderr,"\nToo many errors, giving up\n");
	exit(1);
}

int readwrite_sectors(int fd, /* file descriptor */
		      int drive,
		      enum dir direction,
		      int cylinder, int head, 
		      int sector, int size, /* address */
		      char *data, 
		      int bytes,
		      struct floppy_raw_cmd *raw_cmd,
		      int rate)
{
    raw_cmd->data = data;
    raw_cmd->length = bytes;
    
    raw_cmd->rate = rate;
    raw_cmd->flags = FD_RAW_INTR | FD_RAW_NEED_SEEK | 
	FD_RAW_NEED_DISK | FD_RAW_MORE;

    raw_cmd->cmd_count = 9;  
    
    if (direction == READ_) {
	raw_cmd->cmd[0] = FD_READ & ~0x80;
	raw_cmd->flags |= FD_RAW_READ;
    } else {
	raw_cmd->cmd[0] = FD_WRITE & ~0x80;
	raw_cmd->flags |= FD_RAW_WRITE;
    }

    raw_cmd->cmd[1] = (drive & 3) | (head << 2);
    
    raw_cmd->cmd[2] = cylinder;
    raw_cmd->cmd[3] = head;
    raw_cmd->cmd[4] = sector;
    raw_cmd->cmd[5] = size;
    
    raw_cmd->cmd[6] = sector + (bytes >> (size + 7)) - 1;
    raw_cmd->cmd[7] = 0x1b;
    raw_cmd->cmd[8] = 0xff;

    raw_cmd->track = cylinder;
    return 0;
}

typedef struct sector_map {
	unsigned int head:1;
	unsigned int size:7;
/*	unsigned char sectors;
	unsigned char phantom;*/
	unsigned char position; /* physical position */
} sector_map;


struct xdf_struct {
	unsigned char gap_any;  /* formatting gap */
	unsigned char sect_per_track_any;

	unsigned char skew; /* skew */
	
	/* the following info is used for track 0 */
	unsigned char gap_0; /* gap used on track 0 */
	unsigned char sect_per_track_0;

	/* the following info is used for track 1 (cyl 0 head 1) */
	unsigned char gap_1; /* gap used on track 1 */
	unsigned char sect_per_track_1;


	unsigned char rate;
	unsigned char period;
	unsigned char sizecode;
	unsigned char rootskip;
	unsigned char FatSize;
	sector_map map[9];
} xdf_table[] = {
	{
		/* 5 1/4 format */
		0x29, 19, 21,  74, 16, 36, 17,   0, 42, 0, 0,  9,
		{
			{0,3, 0},
			{0,6, 8},
			{1,2, 1},
			{0,2, 5},
			{1,6, 9},
			{1,3, 4},
			{0,0, 0}
		}
	},
	{
		/* 3 1/2 HD */
		0x7a, 23, 20,  77, 19, 77, 19,   0, 40, 0, 0, 11,
		{
			{0, 3,  0},
			{0, 4,  6},
			{1, 6, 15},
			{0, 2,  4},
			{1, 2,  9},
			{0, 6, 13},
			{1, 4,  2},
			{1, 3, 11},
			{0, 0,  0}
		}
	},
	{
		/* 3 1/2 ED */
		14, 46, 15,  60, 37, 60, 37, 0x43, 60, 1, 1, 22,
		{
			{0,3,   0},
			{0,4,   4},
			{0,5,  11},
			{0,7,  24},
			{1,3,   1},
			{1,4,   5},
			{1,5,  12},
			{1,7,  25},
			{0,0,   0}
		}
	},

	/* my own formats */
	{
		/* 3 1/2 HD */
		56, 24, 25,  49, 20, 49, 20,   0, 50, 0, 1, 12,
		{
			{0, 5,  0},
			{1, 6, 19},
			{0, 6, 17},
			{1, 5,  2},
			{0, 0,  0}
		}
	},

	{
		/* 3 1/2 ED */
		81, 48, 36,  43, 39, 43, 39, 0x43, 54, 1, 0, 21,
		{
			{0, 6,  0},
			{1, 7, 21},
			{0, 7, 20},
			{1, 6,  1},
			{0, 0,  0}
		}
	}

};

#define NUMBER(x) (sizeof(x)/sizeof(x[0]))

#define POS(x) ( (skew + x) % 21 )

static int hs=0;
static int ts=14;
static int debug=0;

void format_track(int fd, int drive, int cylinder, int head, struct xdf_struct *fmt,
		  int FatSize, int RootDirSize)
{
	int i,j;
	int skew;
	int max;
#define NEXT(x,max) x+=2; if(x>=max)x=1;
	struct floppy_raw_cmd raw_cmd;
	sector_map *p;
	format_map_t format_map[256];
	
	for(i=0; i<256; i++){
		format_map[i].sector = i;
		format_map[i].size = fmt->sizecode;
		format_map[i].cylinder = cylinder;
		format_map[i].head = head;
	}
	
	if (cylinder == 0){
		j = 0;
		if(head) {
			for (i=0; i< fmt->sect_per_track_1; i++) {
				format_map[j].sector = i+129;
				format_map[j].size = 2;
				NEXT(j, fmt->sect_per_track_1);
			}
			raw_cmd.cmd[3] = fmt->sect_per_track_1;
			raw_cmd.cmd[4] = fmt->gap_1;
		} else {
			for (i=0; i< 8; i++) {
				format_map[j].sector = i+1;
				format_map[j].size = 2;
				NEXT(j, fmt->sect_per_track_0);
			}
			for (i=0; i< fmt->sect_per_track_0 - 8; i++) {
				format_map[j].sector = i+129;
				format_map[j].size = 2;
				NEXT(j, fmt->sect_per_track_0);
			}
			raw_cmd.cmd[3] = fmt->sect_per_track_0;
			raw_cmd.cmd[4] = fmt->gap_0;
		}
		raw_cmd.cmd[2] = 2;

	} else {
		skew = (cylinder * fmt->skew) % fmt->period;
		max=0;
		for(p = fmt->map; p->size; p++) {
			if(p->head == head) {
				format_map[p->position+skew].sector = p->size+128;
				format_map[p->position+skew].size = p->size;
				if(p->position + skew> max)
					max = p->position + skew;
			}
		}
		raw_cmd.cmd[2] = fmt->sizecode;
		raw_cmd.cmd[3] = max+1;
		raw_cmd.cmd[4] = fmt->gap_any;
	}
	
	raw_cmd.data = format_map;
	raw_cmd.length = sizeof(format_map[0]) * raw_cmd.cmd[3];
	raw_cmd.rate = fmt->rate;
	
	raw_cmd.flags = FD_RAW_INTR | FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK |
		FD_RAW_WRITE;
	
	raw_cmd.cmd[0] = FD_FORMAT;
	raw_cmd.cmd[1] = (drive & 3 ) | ( head << 2 );
	raw_cmd.cmd[5] = 42;
	raw_cmd.cmd_count = 6;  
	raw_cmd.track = cylinder;
	send_cmd(fd, &raw_cmd, "format", 0);
}


enum fs { MAIN_FS, AUX_FS };

void read_sectors(enum fs fs, int sector, char **data, int sectors,
		  int drive, int fd, int cylinder, struct xdf_struct *fmt, 
		  int direction, struct floppy_raw_cmd *raw_cmd, int *j)
{
	int limit, head, psector, lsectors;

	while(sectors) {
		lsectors = sectors;
		head = 0;
		if(fs == MAIN_FS) {
			limit = fmt->sect_per_track_0 - 8;
			if(sector < limit) {
				psector = sector + 129;
				if(sector + lsectors > limit)
					lsectors = limit - sector;
			} else {
				head = 1;
				psector = sector - limit + 129;
			}
		} else {
			if(sector >= 8)
				return;
			psector = sector + 1;
			if(lsectors + sector >= 8)
				lsectors = 8 - sector;
		}
		readwrite_sectors(fd, drive, direction,
				  cylinder, head, psector, 
				  2, *data, lsectors << 9,
				  raw_cmd + (*j)++, fmt->rate);
		*data += lsectors << 9;
		sector += lsectors;
		sectors -= lsectors;
	}
}

#define WORD(x) ((unsigned char)(x)[0] + (((unsigned char)(x)[1]) << 8))

void readwrite_cylinder(int fd, int drive, enum dir direction, 
			int cylinder, char *data, struct xdf_struct *fmt,
			int FatSize, int RootDirSize)
{
    struct floppy_raw_cmd raw_cmd[20];
    int i,j;
    struct timeval tv1, tv2;
    static struct timeval tv3, tv4;
    sector_map *map;

    j=i=0;
    if(cylinder) {
	    map = fmt->map;
	    while(map->size){
		    readwrite_sectors(fd, drive, direction,
				      cylinder, map->head, 
				      map->size + 128, 
				      map->size,
				      data, 128 << map->size, 
				      raw_cmd + j++, fmt->rate);
		    data += 128 << map->size;
		    map++;
		    i++;
	    }
    } else {
	    /* the boot sector & the FAT */
	    read_sectors(MAIN_FS, 0, &data, 1 + FatSize,
			 drive, fd, cylinder, fmt, direction, raw_cmd, &j);
	    
	    /* the index fs */
	    read_sectors(AUX_FS, 0, &data, 8,
			 drive, fd, cylinder, fmt, direction, raw_cmd, &j);

	    if( direction == READ_) {
		    /* the remaining sectors of the phantom FAT */
		    read_sectors(MAIN_FS, 9, &data, FatSize - 8,
				 drive, fd, cylinder, fmt, direction, raw_cmd, &j);
	    } else
		    data += (FatSize - 8) * 512;

	    read_sectors(MAIN_FS, 1 + FatSize, & data, RootDirSize,
			 drive, fd, cylinder, fmt, direction, raw_cmd, &j);

	    if (direction == READ_)
		    read_sectors(AUX_FS, 3, &data, 5,
				 drive,fd,cylinder,fmt, direction, raw_cmd, &j);
	    else
		    data += 5 * 512;
	    read_sectors(MAIN_FS,
			 1 + FatSize + RootDirSize + fmt->rootskip,
			 &data, 
			 fmt->sect_per_track_any * 2 - RootDirSize -
			 2 * FatSize - 6,
			 drive, fd, cylinder, fmt, direction, raw_cmd, &j);
    }

    raw_cmd[j-1].flags &= ~ FD_RAW_MORE;
    if(debug)
	    gettimeofday(&tv1,0);
    send_cmd(fd, raw_cmd, "read/write", 0);
    if(debug) {
	    gettimeofday(&tv2,0);
	    printf("\ncylinder %d: %ld %ld %ld\n\n", cylinder,
		   (long) ((tv2.tv_sec-tv1.tv_sec) * 1000000 + 
			   tv2.tv_usec - tv1.tv_usec),
		   (long) ((tv2.tv_sec-tv1.tv_sec) * 1000000 + 
			   tv2.tv_usec - tv1.tv_usec),
		   (long) ((tv2.tv_sec-tv4.tv_sec) * 1000000 + 
			   tv2.tv_usec - tv4.tv_usec));
	    if(cylinder == 2)
		    tv4 = tv2;
	    tv3 = tv2;
    }
}


static int get_type(int fd)
{
 int drive;
 struct stat statbuf;

 if (fstat(fd, &statbuf) < 0 ){
   perror("stat");
   exit(0);
 }
 
 if (!S_ISBLK(statbuf.st_mode) || major(statbuf.st_rdev) != FLOPPY_MAJOR)
   return -1;

 drive = minor( statbuf.st_rdev );
 return (drive & 3) + ((drive & 0x80) >> 5);
}

#define TRACKSIZE 512*2*48

static void usage(char *progname)
{
    fprintf(stderr,"Usage:\n");
    fprintf(stderr," For copying: %s [-n] <source> <target>\n", progname);
    fprintf(stderr,
	    " For formatting: %s [-D dosdrive] [-t cylinderskew] [-h headskew] <target> [-01234]\n",
	    progname);
    exit(1);
}

static char *readme=
"This is an Xdf disk. To read it in Linux, you have to use a version of\r\n"
"mtools which is more recent than 3.0, and set the environmental\r\n"
"variable MTOOLS_USE_XDF before accessing it.\r\n\r\n"
"Bourne shell syntax (sh, ash, bash, ksh, zsh etc):\r\n"
" export MTOOLS_USE_XDF=1\r\n\r\n"
"C shell syntax (csh and tcsh):\r\n"
" setenv MTOOLS_USE_XDF 1\r\n\r\n"
"mtools can be gotten from http://www.tux.org/pub/knaff/mtools\r\n"
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
    int cylinder, head, ret;
    int c;
    int noformat=0;
    char dosdrive=0;
    int max_cylinder = 80;

    char buffer[TRACKSIZE];
    char cmdbuffer[80];
    char *sourcename, *targetname;
    struct xdf_struct *fmt;
    FILE *fp;
    unsigned int type = 1;
    int FatSize = 11;
    int	RootDirSize = 14;


    while ((c = getopt(argc, argv, "T:t:h:dnD:01234")) != EOF) {
	    switch (c) {
		    case 'D':
			    dosdrive = optarg[0];
			    break;
		    case 't':
			    ts = strtoul(optarg,0,0);
			    break;
		    case 'h':
			    hs = strtoul(optarg,0,0);
			    break;
		    case 'd':
			    debug = 1;
			    break;
		    case 'n':
			    noformat = 1;
			    break;
		    case 'T':
			    max_cylinder = strtoul(optarg,0,0);
			    break;
		    case '0':
		    case '1':
		    case '2':
		    case '3':
		    case '4':
			    type = c - '0';
			    FatSize = xdf_table[type].FatSize;
			    RootDirSize = 14;
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

    if(sourcename) {
	    if( sdrive == -1) {
		    ret = read(sfd, buffer, 512);
		    if ( ret < 0 ){
			    perror("read");
			    exit(1);
		    }
		    if ( ret < 512 ){
			    fprintf(stderr,"short read\n");
			    exit(1);
		    }
		    lseek(sfd, 0, SEEK_SET);
	    } else {
		    struct floppy_raw_cmd raw_cmd;
		    /* the boot sector & the FAT */
		    readwrite_sectors(sfd, sdrive, READ_, 0, 0, 0x81, 2,
				      buffer, 512, &raw_cmd, 0);
		    raw_cmd.flags &= ~ FD_RAW_MORE;
		    if(type == 2 || send_cmd(sfd, &raw_cmd, "probe HD", 1)) {
			    readwrite_sectors(sfd, sdrive, READ_, 0, 0, 0x81, 2,
					      buffer, 512, &raw_cmd, 0x43);
			    raw_cmd.flags &= ~ FD_RAW_MORE;
			    send_cmd(sfd, &raw_cmd, "probe ED", 0);
		    }
	    }
	    RootDirSize = WORD(buffer+17)/16;
	    FatSize = WORD(buffer+22);
	    for(type=0; type < NUMBER(xdf_table); type++) {
		    if(xdf_table[type].sect_per_track_any == WORD(buffer+24))
			    break;
	    }
	    if(type == NUMBER(xdf_table)) {
		    fprintf(stderr,
			    "Source is of unknown density, probably not an XDF disk\n");
		    exit(1);
	    }
    }

    fmt = xdf_table + type;
    for ( cylinder = 0 ; cylinder < max_cylinder ; cylinder++){
	    if(sourcename) {
		    if ( sdrive == - 1 ){
			    ret = read(sfd, buffer, fmt->sect_per_track_any*1024);
			    if ( ret < 0 ){
				    perror("read");
				    exit(1);
			    }
			    if ( ret < fmt->sect_per_track_any * 1024 ){
				    fprintf(stderr,"short read\n");
				    exit(1);
			    }
		    } else {
			    if(progress) {
				    fprintf(stderr,"r%02d", cylinder);
				    fflush(stderr);
			    }
			    readwrite_cylinder(sfd, sdrive, READ_, cylinder, buffer, fmt,
					    FatSize, RootDirSize);
			    clear();
		    }
	    } else {
		    memset(buffer, 0, fmt->sect_per_track_any * 1024);
		    if(!cylinder) {
			    buffer[17]=0xe0;
			    buffer[18]=0;
			    buffer[22]=FatSize;
			    buffer[23]=0;
			    buffer[24]=fmt->sect_per_track_any;
			    buffer[25]=0;
		    }
	    }
	    
	    if (tdrive == -1){
		    ret = write(tfd, buffer, fmt->sect_per_track_any * 1024);
		    if ( ret < 0 ){
			    perror("write");
			    exit(1);
		    }
		    if ( ret < fmt->sect_per_track_any * 1024 ){
			    fprintf(stderr,"short write\n");
			    exit(1);
		    }
	    } else {
		    if(!noformat) {
			    if(progress) {
				    fprintf(stderr,"f%02d", cylinder);
				    fflush(stderr);
			    }
			    for(head = 0; head < 2; head++)
				    format_track(tfd, tdrive, cylinder,head, fmt,
						 FatSize, RootDirSize);
			    clear();
		    }
		    if(progress) {
			    fprintf(stderr,"w%02d", cylinder);
			    fflush(stderr);
		    }
		    readwrite_cylinder(tfd, tdrive, WRITE_, cylinder, buffer, 
				       fmt, FatSize, RootDirSize);
		    clear();
	    }
    }
    close(tfd);
    close(sfd);

    if(dosdrive && !sourcename && !noformat) {
	    snprintf(cmdbuffer,79,"mformat -X -s%d -t%d -h2 %c:",
		    fmt->sect_per_track_any, max_cylinder, dosdrive);
	    system(cmdbuffer);
	    setenv("MTOOLS_USE_XDF","0", 1);
	    snprintf(cmdbuffer,79,"mformat -t1 -h1 -s8 %c:", dosdrive);
	    system(cmdbuffer);
	    snprintf(cmdbuffer,79,"mcopy - %c:README", dosdrive);
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
