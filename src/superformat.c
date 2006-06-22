/*

Done:
	Added calc_skews to figure out skews for all tracks at once.
	This has the happy side effect of making 24-sector formats
	work on my 8272a system whereas it didn't before (perhaps due
	to the time required to calculate skews *during* formatting?)

	Added a few interesting verbosity levels

	verify_now changed to verify_later; verify_now is default

	Combined format_track_1 and format_track_2; once calc_skews
	was created, they took the same parameters and were always
	called in succession

	Add support for 300 Kbps 3.5" DD disks (13-14 sector equivalent) like 2m

	940707 DCN Fixed BYTE vs. CHAR handling of no_verify and verify_later
               Allowed use of 13/14 sector non-2m disks

	940710 AK Fixed support for 300Kbps 3.5" DD disks without 2m

	20021102 AK: switched verify_later to be default on. Newer
	kernels (2.4.x) cannot refrain from reading ahead, even if
	BLKRA is set to 0, and this will cause failure when verifying
	during formatting, because read-ahead will attempt to access
	tracks that are not _yet_ formatted

Todo:
-	Allow reversing cylinder order, or perhaps have option to try as many
	cylinders as happen to work (as in 2m).  Currently, if too many cylinders
	are attempted it won't fail until the very end
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
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/major.h>
#include <errno.h>
#include "enh_options.h"
#include "mediaprm.h"
#include "fdutils.h"
#include "oldfdprm.h"
#include "superformat.h"

int fm_mode=0;

struct defaults {
	char density;
	struct {
		int sect;
		int rate;
	} fmt[6];
} drive_defaults[] = {
{ DENS_UNKNOWN, { {0, 0}, {0, 0} }},
{ DENS_DD, { {0, 0}, { 0, 0}, {9, 2}, { 0, 0}, {0, 0}, {0, 0} } },
{ DENS_HD, { {0, 0}, { 0, 0}, {9, 1}, { 0, 0}, {15, 0}, {0, 0} } },
{ DENS_DD, { {0, 0}, { 0, 0}, {9, 2}, { 0, 0}, {0, 0}, {0, 0} } },
{ DENS_HD, { {0, 0}, { 0, 0}, {9, 2}, { 0, 0}, {18, 0}, {0, 0} } },
{ DENS_ED, { {0, 0}, { 0, 0}, {9, 2}, { 0, 0}, {18, 0}, {36, 0x43} } },
{ DENS_ED, { {0, 0}, { 0, 0}, {9, 2}, { 0, 0}, {18, 0}, {36, 0x43} } } };
int header_size=62;
int index_size=146; 


char floppy_buffer[24 * 512];
int verbosity = 3;
static char noverify = 0;
static char noformat = 0;
static char dosverify = 0;
static char verify_later = 0;
short stretch;
int cylinders, heads, sectors;
int begin_cylinder, end_cylinder;
int head_skew=1024, cylinder_skew=1536, absolute_skew=0;
int use_2m=0;
int lskews[MAX_TRACKS][MAX_HEADS] = {{0}};
int findex[MAX_TRACKS][MAX_HEADS] = {{0}};
int mask= 0;

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

int send_cmd(int fd,struct floppy_raw_cmd *raw_cmd, char *message)
{
	int i;
	int code;
	int retries;
	struct floppy_raw_cmd tmp;
	
	retries=0;
	tmp = *raw_cmd;
 repeat:
	*raw_cmd = tmp;
	if ( ioctl( fd, FDRAWCMD, raw_cmd) < 0 ){
		if ( errno == EBUSY ){
			fprintf(stderr,"FDC busy, sleeping for a second\n");
			sleep(1);
			goto repeat;
		}
		perror(message);
#if 0
		printf("the final skew is %d\n", skew );
#endif
		exit(1);
	}

	if ( raw_cmd->reply_count ){
		switch( raw_cmd->reply[0] & 0xc0 ){
		case 0x40:
			/* ignore EOC */
			if((raw_cmd->reply[0] & ~0x4)== 0x40 &&
			   (raw_cmd->reply[1] == 0x80) &&
			   (raw_cmd->reply[2] == 0)) {
				/* OK, we merely reached the end of 
				 * our cylinder */
				break;
			}

			if((raw_cmd->reply[0] & ~0x4)== 0x40 &&
			   raw_cmd->reply[1] == 0x10 &&
			   raw_cmd->reply[2] == 0x00) {
				fprintf(stderr,"Overrun, pausing for a second\n");
				sleep(1);
				goto repeat;
			}

			fprintf(stderr,"error during command execution\n");
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
			printf("cylinder=%d head=%d sector=%d size=%d\n",
			       raw_cmd->reply[3],
			       raw_cmd->reply[4],
			       raw_cmd->reply[5],
			       raw_cmd->reply[6]);
			return -1;
			break;
		case 0x80:
			fprintf(stderr,"invalid command given\n");
			exit(1);
		case 0xc0:
			fprintf(stderr,"abnormal termination caused by polling\n");
			exit(1);
		case 0:
			/* OK */
			break;
		}
	}
	return 0;
}

int floppy_read(struct params *fd, void *data, int cylinder, 
				int head, int sectors)
{
	int n,m;
	if (lseek(fd->fd, (cylinder * heads + head) * sectors * 512,
		  SEEK_SET) < 0) {
		perror("lseek");
		return -1;
	}
	m = sectors * 512;
	while(m>0) {
		/* read until we have read everything we should */
		n=read(fd->fd, data, m);
		if ( n < 0 ) {
			perror("read");
			return -1;
		}
		if(n== 0) {
			fprintf(stderr, "Error, %d bytes remaining\n", m);
			return -1;
		}
		m -= n;
	}
	return 0;
}


int floppy_write(struct params *fd, void *data, 
				 int cylinder, int head, int sectors)
{
	int n,m;
	if (lseek(fd->fd, (cylinder * heads + head) * sectors * 512,
		  SEEK_SET) < 0) {
		perror("lseek");
		return -1;
	}
	m = sectors * 512;
	while(m>0) {
		/* write until we have write everything we should */
		n=write(fd->fd, data, m);
		if ( n < 0 ) {
			perror("write");
			return -1;
		}
		if(n== 0) {
			fprintf(stderr, "Error, %d bytes remaining\n", m);
			return -1;
		}
		m -= n;
	}
	return 0;
}

int floppy_verify(int superverify, struct params *fd, void *data, 
				  int cylinder, int head, int sectors)
{
	if(floppy_read(fd, data, cylinder, head, sectors))
		return -1;
	
	if(superverify) {
		/* write, and then read again */
		memset(data, sectors * 512, 0x55);
		if(floppy_write(fd, data, cylinder, head, sectors))
			return -1;
		ioctl(fd->fd, FDFLUSH);
		if(floppy_read(fd, data, cylinder, head, sectors))
			return -1;
	}
	return 0;
}

static int rw_track(struct params *fd, int cylinder, int head, int mode);

/* format_track. Does the formatting proper */
int format_track(struct params *fd, int cylinder, int head, int do_skew)
{
	format_map_t *data;
	struct floppy_raw_cmd raw_cmd;
	int offset;
	int i;
	int nssect;      
	int skew;
	
	data = (format_map_t *) floppy_buffer;

	/* place "fill" sectors */
	for (i=0; i<fd->nssect*2+1; ++i){
		data[i].sector = 128+i;
		data[i].size = /*fd->sizecode*/7;
		data[i].cylinder = cylinder;
		data[i].head = head;
	}

	if(do_skew) {
		fd += findex[cylinder][head];
		skew = fd->min + lskews[cylinder][head] * fd->chunksize;
		assert(skew >= fd->min);
		assert(skew <= fd->max);		
	} else
		skew = 0;

	/* place data sectors */
	nssect = 0;
	for (i=0; i<fd->dsect; ++i){
		offset = fd->sequence[i].offset + lskews[cylinder][head];
		offset = offset % fd->nssect;
		data[offset].sector = fd->sequence[i].sect - fd->zeroBased;
		data[offset].size = fd->sequence[i].size;
		data[offset].cylinder = cylinder;
		data[offset].head = head;
		if ( offset >= nssect )
			nssect = offset+1;
	}	
	if ( (nssect-1) * fd->chunksize > fd->raw_capacity - header_size - index_size){
		printf("Sector too far out %d*%d > %d-%d-%d!\n",
		       nssect , fd->chunksize, fd->raw_capacity , header_size,
		       index_size);
		exit(1);
	}

	/* debugging */
	if (verbosity == 9){
		printf("chunksize=%d\n", fd->chunksize);
		printf("sectors=%d\n", nssect);
		for (i=0; i<nssect; ++i)
			printf("%2d/%d, ", data[i].sector, data[i].size);
		printf("\n");
	}

	/* prepare command */
	raw_cmd.data = floppy_buffer;
	raw_cmd.length = nssect * sizeof(format_map_t);
	raw_cmd.cmd_count = 6;
	raw_cmd.cmd[0] = FD_FORMAT & ~fm_mode;
	raw_cmd.cmd[1] = (head << 2 | ( fd->drive & 3)) ^
	    (fd->swapSides ? 4 : 0);
	raw_cmd.cmd[2] = fd->sizecode;
	raw_cmd.cmd[3] = nssect;
	raw_cmd.cmd[4] = fd->fmt_gap;
	raw_cmd.cmd[5] = 0;
	raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR | FD_RAW_SPIN | 
		FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK;
	raw_cmd.track = cylinder << stretch;
	raw_cmd.rate = fd->rate & 0x43;

	/* first pass */
	if (verbosity >= 6)
		printf("formatting...\n");
	if(send_cmd(fd->fd, & raw_cmd, "format"))
		return -1;

	memset(floppy_buffer, 0, sizeof(floppy_buffer));
	if ( !fd->need_init && fd->sizecode)
		return 0;

	if (verbosity >= 6)
		printf("initializing...\n");
	return rw_track(fd, cylinder, head, 1);
}

/* format_track. Does the formatting proper */
static int rw_track(struct params *fd, int cylinder, int head, int mode)
{
	int i;
	int cur_sector;
	int retries;
	struct floppy_raw_cmd raw_cmd;

	cur_sector = 1 - fd->zeroBased;

	for (i=MAX_SIZECODE-1; i>=0; --i) {
		if ( fd->last_sect[i] <= cur_sector + fd->zeroBased)
			continue;
		retries=0;
	retry:
		/* second pass */
		raw_cmd.data = floppy_buffer;
		raw_cmd.cmd_count = 9;
		raw_cmd.cmd[0] =
			(mode ? FD_WRITE : FD_READ) & ~fm_mode & ~0x80;
		raw_cmd.cmd[1] = (head << 2 | ( fd->drive & 3)) ^
		    (fd->swapSides ? 4 : 0);
		raw_cmd.cmd[2] = cylinder;
		raw_cmd.cmd[3] = head;
		raw_cmd.cmd[4] = cur_sector;
		raw_cmd.cmd[5] = i;
		raw_cmd.cmd[6] = fd->last_sect[i] - 1 - fd->zeroBased;
		raw_cmd.cmd[7] = fd->gap;
		if ( i )
			raw_cmd.cmd[8] = 0xff;
		else
			raw_cmd.cmd[8] = 0xff;
		raw_cmd.flags = (mode ? FD_RAW_WRITE : FD_RAW_READ) | 
			FD_RAW_INTR | FD_RAW_SPIN |
			FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK;
		raw_cmd.track = cylinder << stretch;
		raw_cmd.rate = fd->rate & 0x43;

		raw_cmd.length = (fd->last_sect[i] - 
				  fd->zeroBased - 
				  cur_sector) * 128 << i;
		/* debugging */
		if (verbosity == 9)
			printf("%s %ld sectors of size %d starting at %d\n",
			       mode ? "writing" : "reading",
			       raw_cmd.length / 512, i, cur_sector);
		if(send_cmd(fd->fd, & raw_cmd, 
			    mode ? "format" : "verify")){
			if ( !retries && mode && (raw_cmd.reply[1] & ST1_ND) ){
				cur_sector = raw_cmd.reply[5];
				retries++;
				goto retry;
			}
			return -1;
		}
		cur_sector = fd->last_sect[i];
	}
	return 0;
}



void print_formatting(int cylinder, int head)
{
	switch(verbosity) {
		case 0:
			break;
		case 1:
			if (!head) {
				printf(".");
				fflush(stdout);
			}
			break;
		case 2:
			if (head)
				printf("\b=");
			else
				printf("-");
			fflush(stdout);
			break;
		case 3:
		case 4:
			printf("\rFormatting cylinder %2d, head %d ",
				cylinder, head);
			fflush(stdout);
			break;
		default:
			printf("formatting cylinder %d, head %d\n",
				cylinder, head);
			break;
	}
}

void print_verifying(int cylinder, int head)
{
	if (verbosity >= 5) {
		printf("verifying cylinder %d head %d\n",
			cylinder, head);
	} else if (verbosity >= 3) {
		printf("\r Verifying cylinder %2d, head %d ", cylinder, head);
		fflush(stdout);
	} else if (verbosity == 2) {
		if (!verify_later && !dosverify) {
			if (head)
				printf("\b+");
			else
				printf("\bx");
		} else {
			if (head)
				printf("\b+");
			else
				printf("x");
		}
		fflush(stdout);
	}
}


void old_parameters()
{



}

#define DRIVE_DEFAULTS (drive_defaults[drivedesc.type.cmos])

int main(int argc, char **argv)
{
	int nseqs; /* number of sequences used */
	char env_buffer[10];
	struct floppy_struct parameters;
	struct params fd[MAX_SECTORS], fd0;
	int ch,i;
	short density = DENS_UNKNOWN;
	char drivename[10];

	int have_geom = 0;
	int margin=50;
	int deviation=-3100;
	int warmup = 40; /* number of warmup rotations for measurement */

	int cylinder, head, interleave;
	int gap;
	int final_gap;
	int chunksize;
	int sizecode=2;
	int error;
	int biggest_last = 0;
	int superverify=0;

	char command_buffer[80];
	char twom_buffer[6];
	char *progname=argv[0];

	short retries;
	short zeroBased=0;
	short swapSides=0;
	int n,rsize;
	char *verify_buffer = NULL;
	char dosdrive;
	drivedesc_t drivedesc;
	struct floppy_struct geometry;
	int max_chunksize = 128*128+62+256;

	struct enh_options optable[] = {
	{ 'D', "dosdrive", 1, EO_TYPE_CHAR, 0, SET_DOSDRIVE,
		(void *) &dosdrive,
		"set the dos drive" },

	{ 'v', "verbosity", 1, EO_TYPE_LONG, 0, 0,
		(void *) &verbosity,
		"set verbosity level" },

	{ 'f', "noverify", 0, EO_TYPE_BYTE, 1, 0,
		(void *) &noverify,
		"skip verification" },

	{ '\0', "print-drive-deviation", 0, EO_TYPE_BYTE, 1, 0,
		(void *) &noformat,
		"print deviation, do not format " },

	{ 'B', "dosverify", 0, EO_TYPE_BYTE, 1, 0,
		(void *) &dosverify,
		"verify disk using mbadblocks" },

	{ 'V', "verify_later", 1, EO_TYPE_BYTE, 1, 0,
		(void *) &verify_later,
		"verify floppy after all formatting is done" },

	{ 'b', "begin_cylinder", 1, EO_TYPE_LONG, 0, 0,
		(void *) &begin_cylinder,
		"set cylinder where to begin formatting" },

	{ 'e', "end_cylinder", 1, EO_TYPE_LONG, 0, SET_ENDTRACK,
		(void *) &end_cylinder,
		"set cylinder where to end formatting" },

	{ 'G', "fmt_gap", 1, EO_TYPE_LONG, 0, SET_FMTGAP,
		(void *) &gap,
		"set the formatting gap" },

	{ 'F', "final_gap", 1, EO_TYPE_LONG, 0, SET_FINALGAP,
		(void *) &final_gap,
		"set the final gap" },

	{ 'i', "interleave", 1, EO_TYPE_LONG, 0, SET_INTERLEAVE,
		(void *) &interleave,
		"set the interleave factor" },

	{ 'c', "chunksize", 1, EO_TYPE_LONG, 0, SET_CHUNKSIZE,
		(void *) &chunksize,
		"set the size of the \"chunks\" (small auxiliary sectors used for formatting). This size must be in the interval [191,830]" },

	{ '\0', "max-chunksize", 1, EO_TYPE_LONG, 0, 0,
		(void *) &max_chunksize,
		"set a maximal size for the \"chunks\" (small auxiliary sectors used for formatting)." },

	{ 'g', "gap", 1, EO_TYPE_LONG, 0, 0,
		(void *) &fd[0].gap,
		"set the r/w gap" },

	{'\0', "absolute_skew", 1, EO_TYPE_LONG, 0, 0,
		(void *) &absolute_skew,
		"set the skew used at the beginning of formatting" },

	{'\0', "head_skew", 1, EO_TYPE_LONG, 0, 0,
		(void *) &head_skew,
		"set the skew to be added when passing to the second head" },

	{'\0', "cylinder_skew", 1, EO_TYPE_LONG, 0, 0,
		(void *) &cylinder_skew,
		"set the skew to be added when passing to another cylinder" },

	{'\0', "aligned_skew", 0, EO_TYPE_BITMASK_LONG, ALSKEW, 0,
		(void *) &fd[0].flags,
		"select sector aligned skewing" },

	{'w', "warmup", 1, EO_TYPE_LONG, 0, 0,
		 (void *) &warmup,
		 "number of warmup rotations for before measurement of raw drive capacity"},


	{ 'd', "drive", 1, EO_TYPE_STRING, O_RDWR, 0,
		(void *) &fd[0].name,
		"set the target drive (obsolete.  Specify drive without -d instead)" },
	{'\0', "deviation", 1, EO_TYPE_LONG, 0, SET_DEVIATION,
		 (void *) &deviation,
		 "selects the deviation (in ppm) from the standard rotation speed (obsolete. Use " DRIVEPRMFILE " instead"},
	{'m', "margin", 1, EO_TYPE_LONG, 0, SET_MARGIN,
		 (void *) &margin,
		 "selects the margin to be left at the end of the physical cylinder (obsolete. Use " DRIVEPRMFILE " instead)" },
	{ 's', "sectors", 1, EO_TYPE_LONG, 0, SET_SECTORS,
		(void *) &sectors,
		"set number of sectors (obsolete. Use a media description instead)" },

	{ 'H', "heads", 1, EO_TYPE_LONG, 0, SET_HEADS,
		(void *) &heads,
		"set the number of heads (obsolete. Use a media description instead)" },

	{ 't', "cylinders", 1, EO_TYPE_LONG, 0, SET_CYLINDERS,
		(void *) &cylinders,
		"set the number of cylinders (obsolete. Use a media description instead)" },

	{'\0', "stretch", 1, EO_TYPE_LONG, 0, SET_STRETCH,
		(void *) &stretch,
		"set the stretch factor (how spaced the cylinders are from each other) (obsolete. Use a media description instead)" },

	{ 'r', "rate", 1, EO_TYPE_LONG, 0, SET_RATE,
		(void *) &fd[0].rate,
		"set the data transfer rate (obsolete. Use a media description instead)" },

	{ '2', "2m", 0, EO_TYPE_LONG, 255, SET_2M,
		(void *) &use_2m,
		"format disk readable by the DOS 2m shareware program (obsolete. Use a media description instead)" },

	{ '1', "no2m", 0, EO_TYPE_LONG, 0, SET_2M,
		(void *) &use_2m,
		"don't use 2m formatting (obsolete.  Use a media description instead)" },

	{ '\0', "fm", 0, EO_TYPE_SHORT, 0x40, 0,
		(void *) &fm_mode,
		"chose fm mode (obsolete.  Use a media description instead)" },


	{ '\0', "dd", 0, EO_TYPE_SHORT, DENS_DD, 0,
		(void *) &density,
		"chose low density (obsolete.  Use a media description instead)" },

	{ '\0', "hd", 0, EO_TYPE_SHORT, DENS_HD, 0,
		(void *) &density,
		"chose high density (obsolete.  Use a media description instead)" },

	{ '\0', "ed", 0, EO_TYPE_SHORT, DENS_ED, 0,
		(void *) &density,
		"chose extra density (obsolete.  Use a media description instead)" },

	{ 'S', "sizecode", 1, EO_TYPE_LONG, 0, SET_SIZECODE,
		(void *) &sizecode,
		"set the size code of the data sectors. The size code describes the size of the sector, according to the formula size=128<<sizecode. Linux only supports sectors of 512 bytes and bigger. (obsolete.  Use a media description instead)" },

	{ '\0', "biggest-last", 0, EO_TYPE_SHORT, 1, 0,
		(void *) &biggest_last,
		"for MSS formats, make sure that the biggest sector is the last on the track.  This makes superformat more reliable if your drive is slightly out of spec" },

	{ '\0', "superverify", 0, EO_TYPE_SHORT, 1, 0,
		(void *) &superverify,
		"During the verification step, write a pattern of 0x55 to the track, and check whether it can still be read back" },


	{ '\0', "zero-based", 0, EO_TYPE_SHORT, 1, 0,
	  	(void *) &zeroBased,
	  	"Start numbering sectors from 0 instead of 1 (not readable by normal I/O)" },

	{ '\0', 0 }
	};

	/* default values */
	cylinders = 80; heads = 2; sectors = 18;
	fd[0].fd = -1; fd[0].rate = 0; fd[0].flags = 0;
	fd[0].gap = 0x1b;
	dosdrive='\0';
	fd[0].name = 0;
	gap=0;
	sizecode = 2;

	while( (ch=getopt_enh(argc, argv, optable,
			0, &mask, "drive") ) != EOF) {
		if (ch == '?')
			exit(1);
		fprintf(stderr,"unhandled option %d\n", ch);
		exit(1);
	}

	/* sanity checking */
	if (sizecode < 0 || sizecode >= MAX_SIZECODE) {
		fprintf(stderr,"Bad sizecode %d\n", sizecode);
		print_usage(progname,optable, "");
		exit(1);
	}

	if ( gap < 0 ){
		fprintf(stderr,"Fmt gap too small: %d\n", gap);
		print_usage(progname,optable, "");
		exit(1);
	}

	if (sectors <= 0 || cylinders <= 0 || heads <= 0) {
		fprintf(stderr,"bad geometry s=%d h=%d t=%d\n",
			sectors, heads, cylinders);
		print_usage(progname,optable, "");
		exit(1);
	}

	argc -= optind;
	argv += optind;
	if(argc) {
		fd[0].name = argv[0];
		argc--;
		argv++;
	}

	if (! fd[0].name){
		fprintf(stderr,"Which drive?\n");
		print_usage(progname,optable, "");
		exit(1);
	}

	while(1) {
		fd[0].fd = open(fd[0].name, O_RDWR | O_NDELAY | O_EXCL);
		
		/* we open the disk wronly/rdwr in order to check write 
		 * protect */
		if (fd[0].fd < 0) {
			perror("open");
			exit(1);
		}

		if(parse_driveprm(fd[0].fd, &drivedesc))
			exit(1);
		
		fd[0].drive = drivedesc.drivenum;
		fd[0].drvprm = drivedesc.drvprm;

		if(minor(drivedesc.buf.st_rdev) & 0x7c) {
			if(fd[0].name == drivename) {
				fprintf(stderr,
					"%s has bad minor/major numbers\n",
					fd[0].name);
				exit(1);
			}
			/* this is not a generic format device. Close it,
			 * and open the proper device instead */
			if(argc == 0)
				ioctl(fd[0].fd, FDGETPRM, &geometry);
			have_geom = 1;
			close(fd[0].fd);
			snprintf(drivename,9,"/dev/fd%d", fd[0].drive);
			fd[0].name = drivename;
			continue;
		}
		break;
	}




	if(have_geom  ||
	   !parse_mediaprm(argc, argv, &drivedesc, &geometry) ||
	   !parse_fdprm(argc-2, argv+2, &geometry)) {
		if(argc > 0)
			have_geom = 1;
	} else {
		fprintf(stderr,"Syntax error in format description\n");
		exit(1);
	}


	if(have_geom) {
		if(mask & (SET_SECTORS | SET_CYLINDERS | 
			   SET_HEADS | SET_SIZECODE | SET_2M | SET_RATE)) {
			fprintf(stderr,
				"Cannot mix old style and new style geometry spec\n");
			exit(1);
		}

		sectors = geometry.sect;
		cylinders = geometry.track;
		heads = geometry.head;

		fd[0].rate = geometry.rate & 0x43;
		sizecode = (((geometry.rate & 0x38) >> 3) + 2) % 8;
		use_2m = (geometry.rate >> 2) & 1;
		switch(fd[0].rate) {
			case 0x43:
				density = DENS_ED;
				break;
			case 0x2:
			case 0x1:
				density = DENS_DD;
				break;
			case 0:
				density = DENS_HD;
				break;
		}
		stretch = geometry.stretch & 1;
		if(geometry.stretch & FD_ZEROBASED) {
			zeroBased = 1;
		}
		if(geometry.stretch & FD_SWAPSIDES) {
			swapSides = 1;
		}
		mask |= SET_SECTORS | SET_CYLINDERS | 
			SET_SIZECODE | SET_2M | SET_RATE;
	} else {
		/* density */
		if ( (mask & SET_SECTORS ) && density == DENS_UNKNOWN){
			if ( sectors < 15 )
				density = DENS_DD;
			else if ( sectors < 25 )
				density = DENS_HD;
			else
				density = DENS_ED;
		}
		if (density == DENS_UNKNOWN) {
			density = DRIVE_DEFAULTS.density;
			if ( mask & SET_RATE ){
				for (i=0; i< density; ++i) {
					if(fd[0].rate == 
					   DRIVE_DEFAULTS.fmt[i].rate)
						density=i;
				}
			}
		} else {
			if (DRIVE_DEFAULTS.fmt[density].sect == 0) {
				fprintf(stderr,
					"Density %d not supported drive type %d\n",
					density, drivedesc.type.cmos);
				exit(1);
			}
		}

		/* rate */
		if (! ( mask & SET_RATE))
			fd[0].rate = DRIVE_DEFAULTS.fmt[density].rate;
		
		/* number of sectors */
		if (! (mask & SET_SECTORS))
			sectors =DRIVE_DEFAULTS.fmt[density].sect;
		if (! (mask & SET_CYLINDERS)) {
			if (fd[0].drvprm.tracks >= 80)
				cylinders = 80;
			else
				cylinders = 40;
		}

		if ( ! ( mask & SET_STRETCH )){
			if ( cylinders + cylinders < fd[0].drvprm.tracks)
				stretch = 1;
			else
				stretch = 0;
		}
	}

	fd[0].zeroBased = zeroBased;
	fd[0].swapSides = swapSides;
		
	if (cylinders > fd[0].drvprm.tracks) {
		fprintf(stderr,"too many cylinder for this drive\n");
		print_usage(progname,optable,"");
		exit(1);
	}

	if (! (mask & SET_ENDTRACK ) || end_cylinder > cylinders)
		end_cylinder = cylinders;
	if(begin_cylinder >= end_cylinder) {
		fprintf(stderr,"begin cylinder >= end cylinder\n");
		exit(1);
	}

	fd0 = fd[0];
 repeat:
	/* capacity */	
	if (!have_geom && sectors >= 12 && fd[0].rate == 2)
		fd[0].rate = 1;
	switch(fd[0].rate & 0x3) {
	case 0:
		fd[0].raw_capacity = 500 * 1000 / 8 / fd[0].drvprm.rps;
		break;
	case 1:
		fd[0].raw_capacity = 300 * 1000 / 8 / fd[0].drvprm.rps;
		break;
	case 2:
		fd[0].raw_capacity = 250 * 1000 / 8 / fd[0].drvprm.rps;
		break;
	case 3:
		fd[0].raw_capacity = 1000 * 1000 / 8 / fd[0].drvprm.rps;
		break;
	}
	if (fd[0].rate & 0x40)
		header_size = 81;		
	else
		header_size = 62;

	if(! (mask & (SET_DEVIATION | SET_MARGIN)) &&
	   (drivedesc.mask & (1 << FE__DEVIATION))) {	       
		deviation = drivedesc.type.deviation;
		mask |= SET_DEVIATION;
	}

	if(mask & SET_DEVIATION) {
		mask &= ~SET_MARGIN;
		fd[0].raw_capacity +=  fd[0].raw_capacity * deviation / 1000000;
	} else if (mask & SET_MARGIN) {
		fd[0].raw_capacity -= margin;
	} else  {
		int old_capacity = fd[0].raw_capacity;

/*		printf("old capacity=%d\n", old_capacity);*/

		if(verbosity) {
			fprintf(stderr,"Measuring drive %d's raw capacity\n",
				fd[0].drive);
		}
		/* neither a deviation nor a margin have been given */
		fd[0].raw_capacity = measure_raw_capacity(fd[0].fd,
							  fd[0].drive,
							  fd[0].rate,
							  begin_cylinder,
							  warmup,
							  verbosity) / 16;
		if(verbosity) {
			fprintf(stderr,
				"In order to avoid this time consuming "
				"measurement in the future,\n"
				"add the following line to " DRIVEPRMFILE ":\n");
			fprintf(stdout,
				"drive%d: deviation=%d\n",
				fd[0].drive, 
				(fd[0].raw_capacity-old_capacity)*1000000/
				old_capacity);
			fprintf(stderr,
				"CAUTION: The line is drive and controller "
				"specific, so it should be\n" 
				"removed before installing a new "
				"drive %d or floppy controller.\n\n", 
				fd[0].drive);
		}
	}

	if(noformat)
	    return 0;

	/* FIXME.  Why is this needed? */
	fd[0].raw_capacity -= 30;

	fd0.raw_capacity = fd[0].raw_capacity ;

	if (verbosity == 9)
		printf("rate=%d density=%d sectors=%d capacity=%d\n",
			fd[0].rate, density, sectors, fd[0].raw_capacity);

	fd->chunksize = chunksize;
	fd->max_chunksize = max_chunksize;
	fd->preset_interleave = interleave;
	nseqs = compute_all_sequences(fd, sectors * 512, sizecode,
				      gap, mask, biggest_last);

	/* print all the stuff out */
	if (verbosity == 9) {
		for (i=0; i<fd[0].dsect; ++i)
			printf("s=%2d S=%2d o=%2d\n",
				fd[0].sequence[i].sect,
				fd[0].sequence[i].size,
				fd[0].sequence[i].offset);

		printf("fd[0].sizecode=%d\n", fd[0].sizecode);
		printf("fd[0].fmt_gap=%d\n", fd[0].fmt_gap);
		printf("fd[0].dsect=%d\n", fd[0].dsect);
		printf("fd[0].nssect=%d\n", fd[0].nssect);
	}
	if (sizecode > 2 && !(mask & SET_2M)) {
		use_2m = 1;
		if (fd[0].rate == 2) {
			fd[0].rate = 1;
			goto repeat;
		}
	}
	if (use_2m) {
		fd0.dsect = DRIVE_DEFAULTS.fmt[density].sect;
		compute_track0_sequence(&fd0);

		if (verbosity == 9){
			for (i=0; i< fd0.dsect; i++)
				printf("s=%2d S=%2d o=%2d\n",
					fd0.sequence[i].sect,
					fd0.sequence[i].size,
					fd0.sequence[i].offset);

			printf("fd[0].sizecode=%d\n", fd0.sizecode);
			printf("fd[0].fmt_gap=%d\n", fd0.fmt_gap);
			printf("fd[0].dsect=%d\n", fd0.dsect);
			printf("fd[0].nssect=%d\n", fd0.nssect);
		}
	}

	parameters.sect = sectors;
	parameters.head = heads;
	parameters.track = cylinders;
	parameters.size = cylinders * heads * sectors;
	parameters.stretch = stretch 
#ifdef FD_ZEROBASED
		| (zeroBased ? 4 : 0)
#endif
		| (swapSides ? 2 : 0);
	parameters.gap = fd[0].gap;
	if ( !use_2m)
		fd0.rate = fd[0].rate;
	parameters.rate = ( fd0.rate & ~(FD_SIZECODEMASK | FD_2M) ) |
		(((sizecode + 6 ) % 8 ) << 3) | (use_2m ? FD_2M : 0) ;
	parameters.spec1 = 0xcf;
	if(fd[0].fmt_gap < 1 )
		parameters.fmt_gap = 1;
	else if (fd[0].fmt_gap > 255 )
		parameters.fmt_gap = 0;
	else
		parameters.fmt_gap = fd[0].fmt_gap;
	if(ioctl(fd[0].fd, FDSETPRM, &parameters) < 0) {
		perror("set geometry");
		exit(1);
	}

	calc_skews(&fd0, fd, nseqs);

	if (!verify_later && !dosverify) {
		ioctl(fd[0].fd, FDFLUSH );
		verify_buffer = malloc( 512 * sectors * heads);
		if (! verify_buffer) {
			printf("out of memory error\n");
			exit(1);
		}
	}

	retries=0;
	/*ioctl(fd[0].fd, FDRESET, FD_RESET_IF_RAWCMD);*/
	for (cylinder=begin_cylinder; cylinder<end_cylinder;) {
		error=0;
		if ( retries >= 2)
			exit(1);
#if 0
		if(seek_floppy( fd, cylinder << stretch))
			exit(1);
#endif
		for (head=0; head<heads; ++head) {
			print_formatting(cylinder, head);
			if (!cylinder && !head && use_2m){
				/* 2m-style 1st track */
				if (format_track(&fd0, cylinder, head, 0) &&
					format_track(&fd0, cylinder, head, 0))
					exit(1);

			} else {
				/* Everything else */
				if (format_track(fd, cylinder, head, 1) &&
					format_track(fd, cylinder, head, 1))
					exit(1);
			}
		}
		retries++;
		if (!verify_later && !noverify && !dosverify) {
			for (head=0; head<heads; ++head) {
				print_verifying(cylinder, head);
				if (!cylinder && !head && use_2m){
					/* 2m-style 1st track */
					if (rw_track(&fd0, cylinder, head, 0) &&
					    rw_track(&fd0, cylinder, head, 0)){
						error=1;
						break;
					}
				} else {
					/* Everything else */
					if (rw_track(fd, cylinder, head, 0) &&
					    rw_track(fd, cylinder, head, 0)) {
						error=1;
						break;
					}
				}
			}
		}
		if (error){
			ioctl(fd->fd, FDRESET, FD_RESET_ALWAYS);
			continue;
		}
		retries = 0;
		++cylinder;
	}

	ioctl(fd[0].fd, FDFLUSH );
	close(fd[0].fd);

	if (! (mask & SET_DOSDRIVE ) && fd[0].drive < 2 && !zeroBased)
		dosdrive = fd[0].drive+'a';

	if (dosdrive) {
		if (verbosity >= 5)
			printf("calling mformat\n");
		if (use_2m)
			snprintf(twom_buffer, 5, "-2 %2d", fd0.dsect);
		else
			twom_buffer[0]='\0';
		snprintf(command_buffer, 79,
			"mformat -s%d -t%d -h%d -S%d -M512 %s %c:",
			sectors,
			/*use_2m ? sectors : sectors >> (sizecode - 2), */
			cylinders, heads, sizecode, twom_buffer, dosdrive);
		if (verbosity >= 3) {
			printf("\n%s\n", command_buffer);
		}
		snprintf(env_buffer, 9, "%d", (int)fd0.rate&3);
		setenv("MTOOLS_RATE_0", env_buffer,1);
		snprintf(env_buffer, 9, "%d", (int)fd->rate&3);
		setenv("MTOOLS_RATE_ANY", env_buffer,1);
		if(system(command_buffer)){
			fprintf(stderr,"\nwarning: mformat error\n");
			/*exit(1);*/  
			/* Do not fail, if mformat happens to not be 
			 * installed. The user might have wanted to make
			 * an ext2 disk for instance */
			dosverify = 0;
		}			
	} else {
		if(!zeroBased)
			fprintf(stderr,
				"\nwarning: mformat not called because DOS drive unknown\n");
		/*exit(1);*/
		dosverify = 0;
	}

	if (!noverify && verify_later) {
		fd[0].fd = open ( fd[0].name, O_RDONLY);
		if ( fd[0].fd < 0 ) {
			perror("open for verify");
			exit(1);
		}
		if (verbosity >= 7)
			printf("verifying\n");
		else if (verbosity == 2) {
			printf("\r");
			fflush(stdout);
		}
		verify_buffer = malloc( 512 * sectors * heads);
		if (! verify_buffer) {
			printf("out of memory error\n");
			exit(1);
		}
		lseek(fd[0].fd, 512 * begin_cylinder * heads, SEEK_SET );
		for (cylinder=begin_cylinder; cylinder<end_cylinder; ++cylinder)
			for (head=0; head<heads; ++head) {
				print_verifying(cylinder, head);
				rsize = 512 * sectors;
				while(rsize){
					n = read(fd[0].fd,verify_buffer,rsize);
					if ( n < 0){
						perror("read");
						fprintf(stderr, 
							"remaining %d\n", n);
						exit(1);
					}
					if ( n == 0 ){
						fprintf(stderr,"End of file\n");
						exit(1);
					}
					rsize-=n;
				}
			}
		close(fd[0].fd);
	}
	if (dosverify){
		snprintf(command_buffer, 79,
			"mbadblocks %c:", dosdrive);
		if (verbosity >= 3) {
			printf("\n%s\n", command_buffer);
		}
		if(system(command_buffer)){
			fprintf(stderr,"mbadblocks error\n");
			exit(1);
		}			
	}		

	printf("\n");
	return 0;
}
