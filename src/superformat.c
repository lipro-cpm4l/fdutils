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

Todo:
-	Allow reversing track order, or perhaps have option to try as many
	tracks as happen to work (as in 2m).  Currently, if too many tracks
	are attempted it won't fail until the very end
 */

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

#define SET_SOFTGAP 0x1
#define SET_ENDTRACK 0x2
#define SET_CHUNKSIZE 0x4
#define SET_SECTORS 0x8
#define SET_INTERLEAVE 0x10
#define SET_FMTGAP 0x20
#define SET_RATE 0x40
#define SET_SIZECODE 0x80
#define SET_TRACKS 0x100
#define SET_STRETCH 0x200
#define SET_FINALGAP 0x400
#define SET_DOSDRIVE 0x800
#define SET_2M 0x1000
#define SET_VERBOSITY 0x2000
#define SET_MARGIN 0x4000

#define MAX_SIZECODE 8

#define DO_DEBUG 1
#define ALSKEW 4

int margin=36;
int fm_mode=0;
#define MAX_SECTORS 50
/* int skew; */

struct fparm {
	unsigned char track,head,sect,size;
};

struct fparm2 {
	unsigned char sect, size, offset;
};

#define NO_DENSITY -1
#define DOUBLE_DENSITY 0
#define HIGH_DENSITY 1
#define EXTRA_DENSITY 2

struct defaults {
	char density;
	struct {
		int sect;
		int rate;
	} fmt[3];
} drive_defaults[] = {
{ 0, { {0, 0}, {0, 0} }},
{ DOUBLE_DENSITY, { {9, 2}, {0, 0}, {0, 0} } },
{ HIGH_DENSITY, { {9, 1}, {15, 0}, {0, 0} } },
{ DOUBLE_DENSITY, { {9, 2}, {0, 0}, {0, 0} } },
{ HIGH_DENSITY, { {9, 2}, {18, 0}, {0, 0} } },
{ EXTRA_DENSITY, { {9, 2}, {18, 0}, {36, 0x43} } },
{ EXTRA_DENSITY, { {9, 2}, {18, 0}, {36, 0x43} } } };
int header_size=62;
int index_size=146; 

struct params {
	char *name; /* the name of the drive */
	int fd; /* the open file descriptor */
	int drive; /* the drive number */
	int sizecode;
	int rate;
	int gap;
	unsigned int fmt_gap;
	int use_2mf;
	struct fparm2 *sequence;
	int last_sect[MAX_SIZECODE];
	int nssect; /* number of small sectors */
	int dsect; /* number of data sectors */
	int need_init; /* does this track need initialization ? */
	int raw_capacity; /* raw capacity of one track inbytes */
	struct floppy_drive_params drvprm;
	int chunksize; /* used for re-aligning skew */
	int track_end; /* "" */
	int flags;
	int finalgap;
	int multi; /* multiple formats */
	int min;
	int max;
};

char floppy_buffer[24 * 512];
char verbosity = 3;
static char noverify = 0;
static char dosverify = 0;
static char verify_later = 0;
short stretch;
int tracks, heads, sectors;
int begin_track, end_track;
int head_skew=1024, track_skew=1536, absolute_skew=0;
char use_2m=0;
#define MAX_TRACKS 85
#define MAX_HEADS 2
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
#ifdef 0
		printf("the final skew is %d\n", skew );
#endif
		exit(1);
	}

	if ( raw_cmd->reply_count ){
		switch( raw_cmd->reply[0] & 0xc0 ){
		case 0x40:
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
			printf("track=%d head=%d sector=%d size=%d\n",
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

int floppy_read(struct params *fd, void *data, int track, int head, int sectors)
{
	int n;
	if (lseek(fd->fd, (track * heads + head) * sectors * 512,
		  SEEK_SET) < 0) {
		perror("lseek");
		return -1;
	}
	n=read(fd->fd, data, sectors * 512);
	if ( n < 0 )
		perror("read");
	if ( n == sectors * 512)
		return n;
	else
		return -1;
}

void calc_multi_skew(struct params *f, int track, int head, int skew,
		    int *lskew, int *index)
{
	int j;

	for (j=0; ; ++j) {
		if (j == f[0].dsect ) {
			j = 0;
			skew -= f[0].raw_capacity;
		}
		
		if ( f[j].min * f[j].chunksize >= f[j].max )
			continue;
		
		if (skew < f[j].min * f[j].chunksize){
			skew = f[j].min * f[j].chunksize;
			break;
		}
		skew = skew + f[j].chunksize - 1;
		skew -= skew % f[j].chunksize;
		if (skew <= f[j].max){
			if (f[j].flags & ALSKEW )
				continue;
			else
				break;
		}
	}
	f += j;
	if (verbosity == 9)
		printf("format #%d\n", j);
	*index=j;
	if (f->multi && (skew < f->min * f->chunksize || skew >= f->max) ){
		printf("%d %d %d\n",f->min * f->chunksize, skew, f->max);
		exit(1);
	}
	*lskew = skew / f->chunksize;
	*lskew = (*lskew + f->nssect - f->min) % f->nssect;
	if (f->multi && (skew < f->min * f->chunksize ||
			 skew >= f->max) ){
		printf("%d %d %d\n",
		       f->min * f->chunksize, skew, f->max);
		exit(1);
	}
}

void calc_mono_ski(struct params *f,
		  int track, int head, int skew, int *lskew)
{
	int i, track_end, curoffset, maxoffset, aligned;

	track_end = (f->raw_capacity- header_size- index_size- 1) /f->chunksize;
	*lskew = (skew + f->chunksize - 1) / f->chunksize;

	i=aligned=maxoffset=0;
	while(1){
		if ( i == f->dsect ){
			if ( aligned ||  ! (f->flags & ALSKEW) )
				return;
			*lskew += f->nssect - maxoffset;
			i=0;
		}
		curoffset = (f->sequence[i].offset + *lskew) % f->nssect;
		if(curoffset > track_end){
			*lskew = f->nssect - f->sequence[i].offset;
			maxoffset=0;
			aligned = 0;
			i=0;
		}
		if (curoffset == 0 )
			aligned=1;
		if( curoffset > maxoffset)
			maxoffset = curoffset;
		i++;
	}
}

/* calc_skews. Fill skew table for use in formatting */
int calc_skews(struct params *fd0, struct params *fd)
{	
	int track, head;
	struct params *f = NULL;
	int skew, next_track_skew;

	/* Amount to advance skew considering head skew already added in */
	next_track_skew = track_skew - head_skew;
	skew = absolute_skew;

	for (track=begin_track; track <= end_track; ++track) {
		for (head=0; head < heads; ++head) {
			if (!head && !track && use_2m)
				f = fd0;
			else
				f = fd;
			skew = skew % f->raw_capacity;			
			if ( f->multi){
				calc_multi_skew(f, track, head, skew,
						&lskews[track][head],
						&findex[track][head]);
				f += findex[track][head];
			} else {
				calc_mono_ski(f, track, head, skew,
					      &lskews[track][head]);
				findex[track][head]=0;
			}
			skew = (lskews[track][head] + f->track_end) * 
				f->chunksize + head_skew;
		}
		skew += next_track_skew;
	}
	return 0;
} /* End calc_skews */

/* format_track. Does the formatting proper */
int format_track(struct params *fd, int track, int head)
{
	struct fparm *data;
	struct floppy_raw_cmd raw_cmd;
	int offset;
	int i;
	int nssect;      
	int cur_sector;
	int skew;
	int retries;
	
	data = (struct fparm *) floppy_buffer;

	/* place "fill" sectors */
	for (i=0; i<fd->nssect*2+1; ++i){
		data[i].sect = 128+i;
		data[i].size = /*fd->sizecode*/7;
		data[i].track = track;
		data[i].head = head;
	}

	fd += findex[track][head];
	skew = (fd->min + lskews[track][head]) * fd->chunksize;
	if (fd->multi && (skew < fd->min * fd->chunksize ||
			 skew >= fd->max) ){
		printf("%d %d %d\n",
		       fd->min * fd->chunksize, skew, fd->max);
		exit(1);
	}

	/* place data sectors */
	nssect = 0;
	for (i=0; i<fd->dsect; ++i){
		offset = fd->sequence[i].offset + lskews[track][head];
		offset = offset % fd->nssect;
		data[offset].sect = fd->sequence[i].sect;
		data[offset].size = fd->sequence[i].size;
		data[offset].track = track;
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
			printf("%2d/%d, ", data[i].sect, data[i].size);
		printf("\n");
	}

	/* prepare command */
	raw_cmd.data = floppy_buffer;
	raw_cmd.length = nssect * sizeof(struct fparm);
	raw_cmd.cmd_count = 6;
	raw_cmd.cmd[0] = FD_FORMAT & ~fm_mode;
	raw_cmd.cmd[1] = head << 2 | ( fd->drive & 3);
	raw_cmd.cmd[2] = fd->sizecode;
	raw_cmd.cmd[3] = nssect;
	raw_cmd.cmd[4] = fd->fmt_gap;
	raw_cmd.cmd[5] = 0;
	raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR | FD_RAW_SPIN | 
		FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK;
	raw_cmd.track = track << stretch;
	raw_cmd.rate = fd->rate & 0x43;

	/* first pass */
	if (verbosity >= 6)
		printf("formatting...\n");
	if(send_cmd(fd->fd, & raw_cmd, "format"))
		return -1;

	cur_sector = 1;
	memset(floppy_buffer, 0, sizeof(floppy_buffer));
	if ( !fd->need_init && fd->sizecode)
		return 0;

	if (verbosity >= 6)
		printf("initializing...\n");

	for (i=MAX_SIZECODE-1; i>=0; --i) {
		if ( fd->last_sect[i] <= cur_sector )
			continue;

		/* second pass */
		raw_cmd.data = floppy_buffer;
		raw_cmd.cmd_count = 9;
		raw_cmd.cmd[0] = FD_WRITE & ~fm_mode;	
		raw_cmd.cmd[1] = head << 2 | ( fd->drive & 3);
		raw_cmd.cmd[2] = track;
		raw_cmd.cmd[3] = head;
		raw_cmd.cmd[4] = cur_sector;
		raw_cmd.cmd[5] = i;
		raw_cmd.cmd[6] = fd->dsect;
		raw_cmd.cmd[7] = fd->gap;
		if ( i )
			raw_cmd.cmd[8] = 0xff;
		else
			raw_cmd.cmd[8] = 0xff;

		raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR | FD_RAW_SPIN |
			FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK;
		raw_cmd.rate = fd->rate & 0x43;

		retries=0;
	retry:
		raw_cmd.length = (fd->last_sect[i] - cur_sector) * 128 << i;
		/* debugging */
		if (verbosity == 9)
			printf("writing %ld sectors of size %d starting at %d\n",
			       raw_cmd.length / 512, i, cur_sector);
		cur_sector = fd->last_sect[i];
		if(send_cmd(fd->fd, & raw_cmd, "format")){
			if ( !retries && (raw_cmd.reply[1] & ST1_ND) ){
				cur_sector = raw_cmd.reply[5];
				retries++;
				printf("twaddle!\n");
				ioctl(fd->fd, FDTWADDLE);
				goto retry;
			}
			return -1;
		}
	}
	return 0;
}

#define SSIZE(j) ( ( 128 << (j) ) + header_size + soft_gap )
#define FSIZE(j) ( ( 128 << (j) ) + header_size + final_gap )
int compute_chunk_size(int chunksize,
			int sizecode,
			int *chunks_in_sect,
			int soft_gap,
			int final_gap,
			int tailsize,
			int use_multiformat,
			struct params *fd)
{
	int cur_sector;
	int j;
	int minsize=0;

	fd->nssect = 0;
	cur_sector = 1;
	for(j= sizecode; j>=0; j--){
		chunks_in_sect[j] = (SSIZE(j)-1) / chunksize + 1;
		fd->nssect +=chunks_in_sect[j] * (fd->last_sect[j] -cur_sector);
		if ( fd->last_sect[j] != cur_sector)
			minsize = j;
		cur_sector = fd->last_sect[j];
	}

	/* account for last sector */
	fd->nssect -= chunks_in_sect[minsize];
	chunks_in_sect[MAX_SIZECODE] = (FSIZE(minsize)-1)/chunksize+1;
	fd->nssect += chunks_in_sect[MAX_SIZECODE];

	if ( use_multiformat){
		if (tailsize == minsize)
			return (fd->nssect - chunks_in_sect[MAX_SIZECODE])*
				chunksize + FSIZE(tailsize);
		else
			return (fd->nssect - chunks_in_sect[tailsize])*
				chunksize + SSIZE(tailsize);
	} else
		return fd->nssect * chunksize;
}

void compute_sector_sequence(int sectors,
				int chunksize,
				int *sizecode,
				int interleave,
				int soft_gap,
				int final_gap,
				int mask,
				int use_2m,
				int tailsect,
				struct params *fd)
{
	int remaining;
	int cur_sector;
	int nr_sectors;
	int chunks_in_sect[MAX_SIZECODE + 1];
	int i,j;
	int multisize; /* several sizes on same tracks */
	int tot_size;
	int min_tot_size = 0;
	int t_chunksize;
	int ceiling;
	int tailsize = 2;	/* size of last sector before index mark */
	int use_multiformat;
	int max_offset;

 repeat:
	multisize=0;
	fd->need_init = 1;

	/* parameter checking */

	/* distribution of sector sizes */
	for(i=0; i< MAX_SIZECODE; ++i)
		fd->last_sect[i] = 0;
	remaining = sectors;
	cur_sector = 1;

	use_multiformat = 1;
	i = tailsect;
	for (j=*sizecode; j>=0; --j) {
		nr_sectors = remaining << 2 >> j;
		remaining -= nr_sectors << j >> 2;
		cur_sector += nr_sectors;
		fd->last_sect[j] = cur_sector;
		if (nr_sectors > 1)
			use_multiformat = 0;
		if (nr_sectors == 1 && --i == 0)
			tailsize = j;
		if (remaining)
			multisize = 1;
		else if (nr_sectors) {
			if (tailsect==0 || use_multiformat==0)
				tailsize = j;
		}
	}

	fd->dsect = cur_sector-1; /* number of data sectors */
	if (remaining) {
		fprintf(stderr,"Internal error: remaining not 0\n");
#ifdef 0
		printf("the final skew is %d\n", skew );
#endif
		exit(1);
	}

	/* default gap */
	if (! (mask & SET_FMTGAP)) {
		soft_gap = (fd->raw_capacity-sectors*512-margin)/fd->dsect -
			header_size;
		if (soft_gap > 255)
			soft_gap = 255;
	repeat_2:
		if (soft_gap < 1 &&
			!(mask & SET_SIZECODE) &&
			(sectors >= (1 << ( (*sizecode)-1))) &&
			fd->last_sect[*sizecode] > 2 &&
			(sectors % ( 1 << ( (*sizecode)-1)) == 0 ||
			use_2m || !(mask & SET_2M))) {
			(*sizecode)++;
			goto repeat;
		}
		if (soft_gap < -4)
			soft_gap = -4;
	}


	if ( ! (mask & SET_FINALGAP ))
		final_gap = soft_gap;

 repeat_3:
	/* default chunksize */
	if ( ! ( mask & SET_CHUNKSIZE )){
		/* get a default chunksize */
		if (! *sizecode || 
		    (!multisize && soft_gap > 0 && soft_gap <= 256)){
			chunksize = SSIZE(*sizecode);
		} else if ( !multisize ) {
			chunksize = 0;
			ceiling = SSIZE(*sizecode)/( 129 + header_size);
			for(i= 1; i <= ceiling; i++ ){
				t_chunksize = (SSIZE(*sizecode) - 1) / i +1;

				/* bad chunk sizes */
				if (((t_chunksize-header_size-1) & 511) > 255 &&
					t_chunksize > 768 + header_size)
					continue;

				j = (SSIZE(*sizecode) - 1)/t_chunksize + 1;
				tot_size = j * t_chunksize;
				if ( !chunksize || tot_size < min_tot_size ){
					min_tot_size = tot_size;
					chunksize = t_chunksize;
				}
				if ( tot_size == SSIZE(*sizecode) )
					break;
			}
		} else {
			chunksize = 0;
			for (t_chunksize= SSIZE((*sizecode)-1);
				t_chunksize>128+header_size;
				t_chunksize--) {
				for(j=0; j < MAX_SIZECODE; j++ ){
					if(t_chunksize<(128<<j)+header_size+1){
						t_chunksize=(128<<(j-1)) +
							256 + header_size;
						break;
					}
					if ( t_chunksize <= ( 128 << j ) +
					    256 + header_size )
						break;
				}
				tot_size=compute_chunk_size(t_chunksize,
							    *sizecode,
							    chunks_in_sect,
							    soft_gap,final_gap,
							    tailsize,
							    use_multiformat,
							    fd);

				if ( !chunksize || tot_size <= min_tot_size ){
#if 0
					if (verbosity >= 6)
						printf("%d chasing %d\n",
						       t_chunksize, chunksize);
#endif
					min_tot_size = tot_size;
					chunksize = t_chunksize;
				}
			}
		}
	}

	tot_size=compute_chunk_size(chunksize, *sizecode, chunks_in_sect,
					soft_gap, final_gap, tailsize,
					use_multiformat, fd);

	if (fd->raw_capacity - tot_size < margin) {
		if (! (mask & SET_FINALGAP) &&
			  (final_gap > soft_gap || final_gap > 1)) {
			  final_gap-=(margin+1-fd->raw_capacity+tot_size)/2;
			if (final_gap < 1 && soft_gap > 1)
				final_gap = 1;
			goto repeat_3;
		}

		if (! (mask & SET_FMTGAP) &&
			  soft_gap > - 4) {
			  soft_gap -= (margin-fd->raw_capacity+tot_size) /
			  fd->dsect/2+1;
			if ( soft_gap < final_gap )
				final_gap = soft_gap;
			goto repeat_2;
		}

		if (!(mask & SET_SIZECODE) &&
			  (sectors >= (1 << ( (*sizecode)-1))) &&
			  fd->last_sect[*sizecode] > 2) {
			++(*sizecode);
			goto repeat;
		}
	}

	if (tot_size > fd->raw_capacity) {
		fprintf(stderr,"Too many sectors for this disk\n");
		exit(1);
	}

	/* convert chunksize to sizecode/fmt_gap pair */
	for (j=0; j < MAX_SIZECODE; ++j) {
		if (chunksize < (128 << j) + header_size + 1) {
			fprintf(stderr,"Bad chunksize %d\n", chunksize);
#ifdef 0
			printf("the final skew is %d\n", skew );
#endif
			exit(1);
		}
		if (chunksize <= (128 << j) + 256 + header_size) {
			fd->sizecode = j;
			fd->fmt_gap = chunksize - (128 << j) - header_size;
			break;
		}
	}
	if ( j == MAX_SIZECODE ){
		fprintf(stderr,"Chunksize %d too big\n", chunksize );
#ifdef 0
		printf("the final skew is %d\n", skew );
#endif
		exit(1);
	}

	/* default interleave */
	if ( ! (mask & SET_INTERLEAVE) ){
		if ( soft_gap < 32 )
			interleave = 2;
		else
			interleave = 1;
	}

	/* sector sequence, using interleave */
	fd->sequence =(struct fparm2 *) calloc(fd->dsect,sizeof(struct fparm2));
	if ( fd->sequence == 0 ){
		fprintf(stderr,"Out of memory\n");
#ifdef 0
		printf("the final skew is %d\n", skew );
#endif
		exit(1);
	}
	for(i=0; i< fd->dsect; i++)
		fd->sequence[i].sect=0;
	cur_sector = fd->dsect-1;
	if (!use_multiformat)
		tailsect=fd->dsect-1;
	i = tailsect;
	while(1){
		if ( i == 0 )
			i = fd->dsect;
		if ( cur_sector < 0)
			cur_sector += fd->dsect;
		while(fd->sequence[cur_sector].sect ){
			cur_sector--;
			if ( cur_sector < 0 )
				cur_sector += fd->dsect;
		}
		fd->sequence[cur_sector].sect = i;
		j = (*sizecode);
		while( fd->last_sect[j] <= i )
			j--;
		fd->sequence[cur_sector].size = j;
		cur_sector -= interleave;

		i--;
		if ( (i-tailsect) % fd->dsect == 0 )
			break;
	}

	/* now compute the placement in terms of small sectors */
	cur_sector = 0;
	max_offset = 0;
	for(i=0; i< fd->dsect; i++){
		fd->sequence[i].offset = cur_sector;
		if ( cur_sector > max_offset )
			max_offset = cur_sector;
		if ( fd->sequence[i].sect == 1 )
			fd->min = fd->sequence[i].offset;
		if ( fd->sequence[i].sect == fd->dsect)
			cur_sector += chunks_in_sect[MAX_SIZECODE];
		else
			cur_sector += chunks_in_sect[ fd->sequence[i].size ];
		if (fd->sequence[i].sect == fd->dsect)
			fd->track_end = cur_sector;
	}
#if 0
	fd->max = fd->min + chunks_in_sect[tailsize]-2;
#else
	fd->max = fd->raw_capacity + (fd->min - max_offset) * chunksize -
		header_size - index_size - 1 ;
#endif
	if (verbosity == 9){
		printf("chunksize=%d\n", chunksize);
		printf("%d raw bytes per track\n", tot_size );
		printf("%d final gap\n",
			fd->raw_capacity - tot_size );
	}
	fd->chunksize = chunksize;

	if ( !multisize && (*sizecode) == fd->sizecode )
		fd->need_init = 0;
	if ( use_multiformat )
		fd->multi = multisize;
	else
		fd->multi = 0;
}


void compute_track0_sequence(struct params *fd)
{
	int i;
	int sectors;

	sectors= fd->nssect = fd->dsect;

	fd->track_end = 0;
	fd->chunksize = 0x6c + 574;

	fd->need_init = 0;
	fd->sequence =(struct fparm2 *) calloc(fd->dsect,sizeof(struct fparm2));
	if ( fd->sequence == 0 ){
		fprintf(stderr,"Out of memory\n");
#ifdef 0
		printf("the final skew is %d\n", skew );
#endif
		exit(1);
	}

	fd->sizecode = 2;
	if ( fd->rate & 0x40 )
		fd->fmt_gap = 0x54;
	else
		fd->fmt_gap = 0x6c;
	fd->min = 0;

	for(i=0; i<sectors; i++){
		fd->sequence[i].sect = i+1;
		fd->sequence[i].size = 2;
		fd->sequence[i].offset = i;
	}
}

int compar(const void *a, const void *b)
{
	const struct params *ap, *bp;

	ap = (const struct params *)a;
	bp = (const struct params *)b;
	if (ap->min < bp->min)
		return -1;
	if (ap->min == bp->min)
		return 0;
	return 1;
}

void print_formatting(int track, int head)
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
			printf("\rFormatting track %2d, head %d ",
				track, head);
			fflush(stdout);
			break;
		default:
			printf("formatting track %d, head %d\n",
				track, head);
			break;
	}
}

void print_verifying(int track, int head)
{
	if (verbosity >= 5) {
		printf("verifying track %d head %d\n",
			track, head);
	} else if (verbosity >= 3) {
		printf("\r Verifying track %2d, head %d ", track, head);
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

void main(int argc, char **argv)
{
	char env_buffer[10];
	struct floppy_struct parameters;
	struct params fd[MAX_SECTORS], fd0;
	struct stat buf;
	int ch,i;
	short cmos;
	short density=-1;

	int track, head, interleave;
	int soft_gap;
	int final_gap;
	int chunksize;
	int sizecode=2;
	int error;
	char command_buffer[80];
	char twom_buffer[6];
	char *progname=argv[0];

	short retries;
	int n,rsize;
	char *verify_buffer = NULL;
	char dosdrive;

	struct enh_options optable[] = {
	{ 'd', "drive", 1, EO_TYPE_STRING, O_RDWR, 0,
		(void *) &fd[0].name,
		"set the target drive (mandatory parameter)" },

	{ 'D', "dosdrive", 1, EO_TYPE_CHAR, 0, SET_DOSDRIVE,
		(void *) &dosdrive,
		"set the dos drive" },

	{ 's', "sectors", 1, EO_TYPE_LONG, 0, SET_SECTORS,
		(void *) &sectors,
		"set number of sectors" },

	{ 'H', "heads", 1, EO_TYPE_LONG, 0, 0,
		(void *) &heads,
		"set the number of heads" },

	{ 't', "tracks", 1, EO_TYPE_LONG, 0, SET_TRACKS,
		(void *) &tracks,
		"set the number of tracks" },

	{ '\0', "fm", 0, EO_TYPE_SHORT, 0x40, 0,
		(void *) &fm_mode,
		"chose fm mode" },


	{ '\0', "dd", 0, EO_TYPE_SHORT, DOUBLE_DENSITY, 0,
		(void *) &density,
		"chose low density" },


	{ '\0', "hd", 0, EO_TYPE_SHORT, HIGH_DENSITY, 0,
		(void *) &density,
		"chose high density" },

	{ '\0', "ed", 0, EO_TYPE_SHORT, EXTRA_DENSITY, 0,
		(void *) &density,
		"chose extra density" },

	{ 'v', "verbosity", 1, EO_TYPE_LONG, 0, 0,
		(void *) &verbosity,
		"set verbosity level" },

	{ 'f', "noverify", 0, EO_TYPE_BYTE, 1, 0,
		(void *) &noverify,
		"skip verification" },

	{ 'B', "dosverify", 0, EO_TYPE_BYTE, 1, 0,
		(void *) &dosverify,
		"verify disk using mbadblocks" },

	{ 'V', "verify_later", 0, EO_TYPE_BYTE, 1, 0,
		(void *) &verify_later,
		"verify floppy after all formatting is done" },

	{ 'b', "begin_track", 1, EO_TYPE_LONG, 0, 0,
		(void *) &begin_track,
		"set track where to begin formatting" },

	{ 'e', "end_track", 1, EO_TYPE_LONG, 0, SET_ENDTRACK,
		(void *) &end_track,
		"set track where to end formatting" },

	{ 'S', "sizecode", 1, EO_TYPE_LONG, 0, SET_SIZECODE,
		(void *) &sizecode,
		"set the size code of the data sectors. The size code describes the size of the sector, according to the formula size=128<<sizecode. Linux only supports sectors of 512 bytes and bigger." },

	{ 'G', "fmt_gap", 1, EO_TYPE_LONG, 0, SET_FMTGAP,
		(void *) &soft_gap,
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


	{ 'g', "gap", 1, EO_TYPE_LONG, 0, 0,
		(void *) &fd[0].gap,
		"set the r/w gap" },

	{ 'r', "rate", 1, EO_TYPE_LONG, 0, SET_RATE,
		(void *) &fd[0].rate,
		"set the data transfer rate" },

	{ '2', "2m", 0, EO_TYPE_BYTE, 255, SET_2M,
		(void *) &use_2m,
		"format disk readable by the DOS 2m shareware program" },

	{ '1', "no2m", 0, EO_TYPE_BYTE, 0, SET_2M,
		(void *) &use_2m,
		"don't use 2m formatting" },

	{'\0', "absolute_skew", 1, EO_TYPE_LONG, 0, 0,
		(void *) &absolute_skew,
		"set the skew used at the beginning of formatting" },

	{'\0', "head_skew", 1, EO_TYPE_LONG, 0, 0,
		(void *) &head_skew,
		"set the skew to be added when passing to the second head" },

	{'\0', "track_skew", 1, EO_TYPE_LONG, 0, 0,
		(void *) &track_skew,
		"set the skew to be added when passing to another cylinder" },

	{'\0', "stretch", 1, EO_TYPE_LONG, 0, SET_STRETCH,
		(void *) &stretch,
		"set the stretch factor (how spaced the tracks are from each other)" },
	{'\0', "aligned_skew", 0, EO_TYPE_BITMASK_LONG, ALSKEW, 0,
		(void *) &fd[0].flags,
		"select sector aligned skewing" },
	{'m', "margin", 1, EO_TYPE_LONG, 0, SET_MARGIN,
		 (void *) &margin,
		 "selects the margin to be left at the end of the physical track", },
	{ '\0', 0 }
	};

	/* default values */
	tracks = 80; heads = 2; sectors = 18;
	fd[0].fd = -1; fd[0].rate = 0; fd[0].flags = 0;
	fd[0].gap = 0x1c;
	dosdrive='\0';
	fd[0].name = 0;
	soft_gap=0;
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

	if ( soft_gap < -60 ){
		fprintf(stderr,"Soft gap too small: %d\n", soft_gap);
		print_usage(progname,optable, "");
		exit(1);
	}

	if (sectors <= 0 || tracks <= 0 || heads <= 0) {
		fprintf(stderr,"bad geometry s=%d h=%d t=%d\n",
			sectors, heads, tracks);
		print_usage(progname,optable, "");
		exit(1);
	}

	argc -= optind;
	argv += optind;
	if ( argc > 1){
		print_usage(progname,optable, "");
		exit(1);
	}
	if(argc)
		fd[0].name = argv[0];

	if (! fd[0].name){
	  fprintf(stderr,"Which drive?\n");
	  print_usage(progname,optable, "");
	  exit(1);
	}
			
	fd[0].fd = open(fd[0].name, O_RDWR | O_NDELAY | O_EXCL);

	/* we open the disk wronly/rdwr in order to check write protect */
	if (fd[0].fd < 0) {
		perror("open");
		exit(1);
	}
	ioctl(fd[0].fd, FDRESET, FD_RESET_IF_RAWCMD);
	if (fstat (fd[0].fd, &buf) < 0) {
		perror("fstat");
		exit(1);
	}
	if (MAJOR(buf.st_rdev) != FLOPPY_MAJOR) {
		fprintf(stderr,"%s is not a floppy drive\n", fd[0].name);
		exit(1);
	}
	fd[0].drive = MINOR( buf.st_rdev );
	fd[0].drive = (fd[0].drive & 3) + ((fd[0].drive & 0x80) >> 5);

	if (ioctl(fd[0].fd, FDGETDRVPRM, & fd[0].drvprm ) < 0 ){
		perror("get drive characteristics");
		exit(1);
	}
	cmos = fd[0].drvprm.cmos;
	if (cmos < 1 || cmos > 6)
		cmos = 4;

	/* density */
	if ( (mask & SET_SECTORS ) && density == NO_DENSITY ){
		if ( sectors < 15 )
			density = DOUBLE_DENSITY;
		else if ( sectors < 25 )
			density = HIGH_DENSITY;
		else
			density = EXTRA_DENSITY;
	}
	if (density == NO_DENSITY) {
		density = drive_defaults[cmos].density;
		if ( mask & SET_RATE ){
			for (i=0; i< density; ++i) {
				if(fd[0].rate == drive_defaults[cmos].fmt[i].rate)
					density=i;
			}
		}
	} else {
		if (drive_defaults[cmos].fmt[density].sect == 0) {
			fprintf(stderr,
				"Density %d not supported drive type %d\n",
				density, cmos);
			exit(1);
		}
	}

	/* rate */
	if (! ( mask & SET_RATE))
		fd[0].rate = drive_defaults[cmos].fmt[density].rate;

	/* number of sectors */
	if (! (mask & SET_SECTORS))
		sectors =drive_defaults[cmos].fmt[density].sect;

	fd0 = fd[0];
 repeat:
	/* capacity */
	if (sectors >= 12 && fd[0].rate == 2)
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
	if ( ! (mask & SET_MARGIN)) {
		if (fd[0].rate & 0x40)
			margin = 72;		
		else
			margin = 45; /*36;*/
	}		

	fd0.raw_capacity = fd[0].raw_capacity ;

	if (verbosity == 9)
		printf("rate=%d density=%d sectors=%d capacity=%d\n",
			fd[0].rate, density, sectors, fd[0].raw_capacity);

	fd0.multi=0;
	fd[0].multi = 1;

	compute_sector_sequence(sectors, chunksize, &sizecode,
				interleave, soft_gap, final_gap, mask,
				use_2m, 1, fd);

	if (fd->multi) {
		if (fd[0].dsect > MAX_SECTORS) {
			fprintf(stderr,"Internal error, too many data sectors for multiformat\n");
			exit(1);
		}
		for (i=2; i <= fd[0].dsect; ++i) {
			fd[i-1] = fd[0];
			compute_sector_sequence(sectors, chunksize, &sizecode,
						interleave, soft_gap,
						final_gap, mask | SET_SIZECODE,
						use_2m, i, fd+(i-1));
		}
		qsort( fd, fd[0].dsect, sizeof(struct params), compar);
	}

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
		fd0.dsect = drive_defaults[cmos].fmt[density].sect;
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

	if (! (mask & SET_TRACKS)) {
		if (fd[0].drvprm.tracks >= 80)
			tracks = 80;
		else
			tracks = 40;
	}

	if (tracks > fd[0].drvprm.tracks) {
		fprintf(stderr,"too many track for this drive\n");
		print_usage(progname,optable,"");
		exit(1);
	}

	if ( ! ( mask & SET_STRETCH )){
		if ( tracks + tracks < fd[0].drvprm.tracks )
			stretch = 1;
		else
			stretch = 0;

	}

	if (! (mask & SET_ENDTRACK ) || end_track > tracks)
		end_track = tracks;

	parameters.sect = sectors;
	parameters.head = heads;
	parameters.track = tracks;
	parameters.size = tracks * heads * sectors;
	parameters.stretch = stretch;
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

	calc_skews(&fd0, fd);

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
	for (track=begin_track; track<end_track;) {
		error=0;
		if ( retries >= 2)
			exit(1);
#if 0
		if(seek_floppy( fd, track << stretch))
			exit(1);
#endif
		for (head=0; head<heads; ++head) {
			print_formatting(track, head);
			if (!track && !head && use_2m){
				/* 2m-style 1st track */
				if (format_track(&fd0, track, head) &&
					format_track(&fd0, track, head))
					exit(1);

			} else {
				/* Everything else */
				if (format_track(fd, track, head) &&
					format_track(fd, track, head))
					exit(1);
			}
		}
		retries++;
		if (!verify_later && !noverify && !dosverify) {
			for (head=0; head<heads; ++head) {
				print_verifying(track, head);
				if (!track && !head && use_2m)
					n = floppy_read(&fd0,
							(void *)verify_buffer,
							track, head, fd0.dsect);
				else
					n = floppy_read(fd,
							(void *)verify_buffer,
							track, head, sectors);
				if (n < 0) {
					perror("read");
					error=1;
					fprintf(stderr, "remaining %d\n", n);
					continue;
				}
				if (n == 0) {
					error = 1;
					fprintf(stderr,"End of file\n");
					continue;
				}
			}
		}
		if (error){
			ioctl(fd->fd, FDRESET, FD_RESET_ALWAYS);
			continue;
		}
		retries = 0;
		++track;
	}

	ioctl(fd[0].fd, FDFLUSH );
	close(fd[0].fd);

	if (! (mask & SET_DOSDRIVE ) && fd[0].drive < 2)
		dosdrive = fd[0].drive+'a';

	if (dosdrive) {
		if (verbosity >= 5)
			printf("calling mformat\n");
		if (use_2m)
			sprintf(twom_buffer, "-2 %2d", fd0.dsect);
		else
			twom_buffer[0]='\0';
		sprintf(command_buffer,
			"mformat -s%d -t%d -h%d -S%d -M512 %s %c:",
			sectors,
			/*use_2m ? sectors : sectors >> (sizecode - 2), */
			tracks, heads, sizecode, twom_buffer, dosdrive);
		if (verbosity >= 3) {
			printf("\n%s\n", command_buffer);
		}
		sprintf(env_buffer,"%d", (int)fd0.rate&3);
		setenv("MTOOLS_RATE_0", env_buffer,1);
		sprintf(env_buffer,"%d", (int)fd->rate&3);
		setenv("MTOOLS_RATE_ANY", env_buffer,1);
		if(system(command_buffer)){
			fprintf(stderr,"mformat error\n");
			exit(1);
		}			
	} else {
		fprintf(stderr,"mformat not called because DOS drive unknown\n");
		exit(1);
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
		lseek(fd[0].fd, 512 * begin_track * heads, SEEK_SET );
		for (track=begin_track; track<end_track; ++track)
			for (head=0; head<heads; ++head) {
				print_verifying(track, head);
				rsize = 512 * sectors;
				while(rsize){
					n = read(fd[0].fd,verify_buffer,rsize);
					if ( n < 0){
						perror("read");
						fprintf(stderr, "remaining %d\n", n);
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
		sprintf(command_buffer,
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
	exit(0);
}
