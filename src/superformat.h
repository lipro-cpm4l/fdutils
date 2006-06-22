#ifndef SUPERFORMAT_H
#define SUPERFORMAT_H

#include "driveprm.h"


#define SET_SOFTGAP 0x1
#define SET_ENDTRACK 0x2
#define SET_CHUNKSIZE 0x4
#define SET_SECTORS 0x8
#define SET_INTERLEAVE 0x10
#define SET_FMTGAP 0x20
#define SET_RATE 0x40
#define SET_SIZECODE 0x80
#define SET_CYLINDERS 0x100
#define SET_STRETCH 0x200
#define SET_FINALGAP 0x400
#define SET_DOSDRIVE 0x800
#define SET_2M 0x1000
#define SET_VERBOSITY 0x2000
#define SET_MARGIN 0x4000
#define SET_DEVIATION 0x8000
#define SET_HEADS 0x10000

#define MAX_SIZECODE 8

#define DO_DEBUG 1
#define ALSKEW 4

#define MAX_TRACKS 85
#define MAX_HEADS 2

extern int fm_mode;
extern int cylinder_skew;
extern int head_skew;
extern int absolute_skew;
extern int begin_cylinder;
extern int end_cylinder;
extern int heads;
extern int sectors;
extern int use_2m;

extern int lskews[MAX_TRACKS][MAX_HEADS];
extern int findex[MAX_TRACKS][MAX_HEADS];


#define MAX_SECTORS 50
/* int skew; */

struct fparm {
	unsigned char cylinder,head,sect,size;
};

struct fparm2 {
	unsigned char sect, size, offset;
};


struct params {
	char *name; /* the name of the drive */
	int fd; /* the open file descriptor */
	int drive; /* the drive number */
	int sizecode;
	int preset_interleave;
	int actual_interleave;
	int rate;
	int gap;
	unsigned int fmt_gap;
	int use_2mf;
	struct fparm2 *sequence;
	int last_sect[MAX_SIZECODE];
	int nssect; /* number of small sectors */
	int dsect; /* number of data sectors */
	int need_init; /* does this cylinder need initialization ? */
	int raw_capacity; /* raw capacity of one track inbytes */
	struct floppy_drive_params drvprm;
	int chunksize; /* used for re-aligning skew */
	int max_chunksize;

	int flags;
	int min; /* minimal offset that this geometry can handle */
	int max; /* maximal offset */
	int length; /* length of the track */
	int rotations; /* how many time do we have to go over 0 to read
			* the track */
	int zeroBased; /* 1 if sector numbering starts at zero */
	int swapSides; /* if logical side 0 is on physical 1 and vice-versa */
};


int compute_all_sequences(struct params *fd, 
			  int tracksize,
			  int sizecode,
			  int gap,
			  int mask,
			  int biggest_last);
void compute_track0_sequence(struct params *fd);
int calc_skews(struct params *fd0, struct params *fd, int n);
extern int verbosity;
int header_size;
int index_size;

#endif
