#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "superformat.h"
#include "fdutils.h"

#define GAP_DIVISOR (128*128)

#define GAPSIZE(j) (( (128<<j) * gap + GAP_DIVISOR/2 ) / GAP_DIVISOR)
#define SSIZE(j)   ( (128<<j) + GAPSIZE(j) + header_size)


static inline int chunks_in_sect(struct params *fd, int i, 
				 int gap, int chunksize)
{
	return (SSIZE(i)-1) / chunksize + 1;
}

static inline int sizeOfSector(struct params *fd, int i)
{
	int j;

	if(i < 0)
		return -1; /* reserved value, meaning no multisize */

	for(j = MAX_SIZECODE-1; j >= 0 && fd->last_sect[j] <= i; j--);
	return j;
}


static inline int firstSector(struct params *fd, int i)
{
	if(i>=MAX_SIZECODE-1)
		return 1;
	else
		return fd->last_sect[i+1];
}

static inline int lastSector(struct params *fd, int i)
{
	return fd->last_sect[i];
}

static inline int nrSectorsForSize(struct params *fd, int i)
{
	return  lastSector(fd, i) - firstSector(fd, i);
}


static inline int isMultisize(struct params *fd)
{
	int i, n;

	n = 0;
	for(i = 1; i < MAX_SIZECODE; i++) {
		if(nrSectorsForSize(fd, i))
			n++;
	}
	return n > 1;
}

static int compute_tot_size(struct params *fd,
			    int chunksize,
			    int gap,
			    int tailsize)
{
	int i, nr;

	fd->nssect = 0;
	fd->actual_interleave = 1;
	for(i= 0; i < MAX_SIZECODE; i++){
		nr = nrSectorsForSize(fd, i);
		fd->nssect += chunks_in_sect(fd, i, gap, chunksize) * nr;
		if(nr && GAPSIZE(i) < 34)
			fd->actual_interleave = 2;
	}

	if (tailsize >= 0)
		return (fd->nssect - 
			chunks_in_sect(fd, tailsize, gap, chunksize)) *
			chunksize + SSIZE(tailsize);
	else
		return fd->nssect * chunksize;
}

/* find out how many sectors of each size there are */
static void compute_sizes(struct params *fd, 
			  int remaining, /* bytes per track */
			  int max_sizecode) /* size of biggest sector used */
{
	int cur_sector;
	int sizes; /* number of different sizes found along the track */
	int i;
	int nr_sectors;

	cur_sector = 1;
	sizes=0;
	for (i=MAX_SIZECODE-1; i>=0; --i) {
		if(i > max_sizecode)
			nr_sectors = 0;
		else {			
			nr_sectors = remaining >> (i+7);
			remaining -= nr_sectors << (i+7);
		}
		cur_sector += nr_sectors;
		fd->last_sect[i] = cur_sector;
		if(nr_sectors)
			sizes++;
	}
	fd->dsect = cur_sector-1; /* number of data sectors */
	if(sizes > 1)
		fd->need_init = 1;

	if (remaining) {
		fprintf(stderr,"Internal error: remaining not 0\n");
		abort();
	}
}

static int compute_gap(struct params *fd, int track_size)
{
	int gap;

	gap = (fd->raw_capacity-track_size)*track_size/GAP_DIVISOR-header_size;
	if (gap > 0x6c * 32)
		/* kludge to allow to format at least the usual
		 * formats on out of tolerance drives */
		gap = 0x6c * 32;		
	if (gap < 0)
		gap = 0;
	return gap;
}

/*
 * Determine the chunk size for disks which have same sized sectors.
 * We do this by dividing the sector size through ever increasing
 * divisors.  t_chunksize = ceil( sector_size / divisor )
 * We skip divisors yielding unreachable chunk sizes.
 */

static int compute_chunk_size_for_monosize(struct params *fd,
					   int gap,
					   int tailsize)
{
	int min_chunksize; /* minimal chunk size reached so far */
	int t_chunksize; /* tentative chunk size */
	int ceiling; /* maximal divisor */
	int t_sect_size; /* tentative sector size */
	int min_sect_size=0; /* actual sector size */
	int sector_size;
	int chunks_per_sect;
	int i;

	sector_size = SSIZE(sizeOfSector(fd,1));
	min_chunksize = 0;
	ceiling = sector_size /( 129 + header_size);
	for(i= 1; i <= ceiling; i++ ){
		t_chunksize = (sector_size - 1)/i + 1;
		
		/* unreachable chunk sizes */
		if (((t_chunksize-header_size-1) & 511) > 255 &&
		    t_chunksize > 768 + header_size)
			continue;
		
		chunks_per_sect = (sector_size - 1)/t_chunksize + 1;
		t_sect_size = chunks_per_sect * t_chunksize;

		/* find the smallest sector size */
		if (!min_chunksize || t_sect_size < min_sect_size ){
			min_sect_size = t_sect_size;
			min_chunksize = t_chunksize;
		}
		if (t_sect_size == sector_size)
			break;
	}
	if(min_chunksize != sector_size)
		fd->need_init = 1;
	return  min_chunksize;
}


static int compute_chunk_size_for_multisize(struct params *fd,
					    int gap,
					    int tailsize)
{
	int t_chunksize;
	int tot_size;
	int min_chunksize;
	int i;
	int min_tot_size = 0;

	min_chunksize = 0;
	fd->need_init = 1;
	for(t_chunksize = fd->max_chunksize; 
	    t_chunksize > 128+header_size; 
	    t_chunksize--){
		for(i=0; i < MAX_SIZECODE; i++ ){
			if(t_chunksize<(128<<i)+header_size+1){
				t_chunksize=(128<<(i-1)) +
					256 + header_size;
				break;
			}
			if (t_chunksize <= (128 << i) + 256 + header_size)
				break;
		}
		tot_size = compute_tot_size(fd, t_chunksize, gap, tailsize);
		if ( !min_chunksize || tot_size <= min_tot_size ){
#if 0
			if (verbosity >= 6)
				printf("%d chasing %d\n",
				       t_chunksize, min_chunksize);
#endif
			min_tot_size = tot_size;
			min_chunksize = t_chunksize;
		}
	}
	return min_chunksize;
}



static void compute_chunk_size(struct params *fd,
			      int gap,
			      int tailsize)

{
	if (isMultisize(fd))
		fd->chunksize = compute_chunk_size_for_multisize(fd, gap, 
								 tailsize);
	else
		fd->chunksize = compute_chunk_size_for_monosize(fd, gap, 
								tailsize);
}


/* convert chunksize to sizecode/fmt_gap pair */
static void convert_chunksize(struct params *fd)
{
	int i;


	for (i=0; i < MAX_SIZECODE; ++i) {
		if (fd->chunksize < (128 << i) + header_size + 1) {
			fprintf(stderr,"Bad chunksize %d\n", fd->chunksize);
			exit(1);
		}
		if (fd->chunksize <= (128 << i) + 256 + header_size) {
			fd->sizecode = i;
			fd->fmt_gap = fd->chunksize - (128 << i) - header_size;
			break;
		}
	}
	if (i == MAX_SIZECODE){
		fprintf(stderr,"Chunksize %d too big\n", fd->chunksize );
		exit(1);
	}
}



/*
 * calculate the ordering of the sectors along the track in such
 * a way that the last one is sector number <tailsect>
 */
static void calc_sequence(struct params *fd, int tailsect)
{
	int sec_id, cur_sector, i;

	fd->sequence = SafeNewArray(fd->dsect,struct fparm2);
	cur_sector = fd->dsect-1;

	/* construct the sequence while working backwards.  cur_sector
	 * points to the place where the next sector will be placed.
	 * We place it, then move circularily backwards placing more
	 * and more sectors */
	sec_id = tailsect;
	fd->rotations = 0;
	for(i=0; i < fd->dsect; 
	    i++, cur_sector -= fd->actual_interleave, sec_id--) {
		if (sec_id == 0)
			sec_id = fd->dsect;

		if ( cur_sector < 0) {
			cur_sector += fd->dsect;
			if(sec_id != fd->dsect)
				fd->rotations++;
		}
			
		/* slot occupied, look elsewhere */
		while(fd->sequence[cur_sector].sect ){
			cur_sector--;
			if ( cur_sector < 0 ) {
				cur_sector += fd->dsect;
				if(sec_id != fd->dsect)
					fd->rotations++;
			}
		}

		/* place the sector */
		fd->sequence[cur_sector].sect = sec_id;
		fd->sequence[cur_sector].size = sizeOfSector(fd, sec_id);
	}

	/* handle wrap-around between tailsect and tailsect+1 */
	if(tailsect != fd->dsect) {
		/* always add one rotation, because tailsect+1 cannot be
		 * at the last position, thus is necessarily earlyer */
		fd->rotations++;
		
		if(fd->actual_interleave == 2 && 
		   cur_sector + fd->actual_interleave == 1)
			/* if we use interleave, and the last sector was
			 * placed at the first last position, add one
			 * extra rotation for tailsect+1 following tailsect
			 * too closely */
			fd->rotations++;
	}
}


/* given the sequence, calculate the exact placement of the sectors */
static void calc_placement(struct params *fd, int gap)
{
	int cur_sector, i, max_offset;
	int track_end=0;
	int final_slack; /* slack space extending from data start of last
			  * sector on the track to fd->raw_capacity mark */

	/* now compute the placement in terms of small sectors */
	cur_sector = 0;
	for(i=0; i< fd->dsect; i++){
		fd->sequence[i].offset = cur_sector;
		max_offset = cur_sector;

		/* offset of the starting sector */
		if ( fd->sequence[i].sect == 1 )
			fd->min = cur_sector * fd->chunksize;

		/* offset of the end of the of the highest sector */
		if (fd->sequence[i].sect == fd->dsect)
			track_end = cur_sector * fd->chunksize + 
				header_size + index_size +
				SSIZE(fd->sequence[i].size);

		if(i == fd->dsect - 1)
			break;

		cur_sector += chunks_in_sect(fd, 
					     fd->sequence[i].size, 
					     gap, fd->chunksize);
	}
	final_slack = fd->raw_capacity - cur_sector * fd->chunksize -
		header_size - index_size - 1;
	if(final_slack < 0) {
		fprintf(stderr,
			"Internal error, negative final slack %d\n",
			final_slack);
		abort();
	}
	fd->max = fd->min + final_slack;

	fd->length = fd->rotations * fd->raw_capacity + track_end - fd->min;
	if(fd->length < 0) {
		fprintf(stderr,
			"Internal error, negative track length %d %d %d\n",
			fd->length, track_end, fd->min);
		abort();
		exit(1);
	}


	/* this format accepts any offsets ranging from fd->min to fd->max.
	 * After this track, the current offset will be:
	 *  fd->track_end + initial_offset - fd->min
	 */
}


static int compute_chunks_per_sect(struct params *fd,
				    int tracksize,
				    int sizecode,
				    int *gap,
				    int mask,
				    int tailsize)
{
	int tot_size;

	if (! (mask & SET_FMTGAP))
		*gap = compute_gap(fd, tracksize);	
	while(1) {
		compute_chunk_size(fd, *gap, tailsize);
		tot_size=compute_tot_size(fd, fd->chunksize, *gap, tailsize);
		if(fd->raw_capacity >= tot_size)
			/* enough space available, ok */
			break;
		if ((mask & SET_FMTGAP) || *gap <= 0)
			/* does not fit on disk */
			return -1;

		*gap -= (tot_size-fd->raw_capacity) * GAP_DIVISOR / tracksize;
		if (*gap < 0)
			*gap = 0;
	}

	convert_chunksize(fd);
	
	if (mask & SET_INTERLEAVE)
		fd->actual_interleave = fd->preset_interleave;

	if(verbosity >= 9) {
		printf("%d raw bytes per cylinder\n", tot_size );
		printf("%d final gap\n",
			fd->raw_capacity - tot_size );
	}
	return 0;
}

static void compute_sector_sequence(struct params *fd, int tailsect, int gap)
{
	calc_sequence(fd, tailsect);
	calc_placement(fd, gap);

	if (verbosity >= 9)
		printf("chunksize=%d\n", fd->chunksize);
}


static void compute_all_sequences_for_size(struct params *fd,
					   int *offset,
					   int tracksize,
					   int sizecode,
					   int gap,
					   int mask,
					   int tailsize)
{
	int base = *offset;
	int i;

	/* no sectors of this size */
	if(!nrSectorsForSize(fd, tailsize))
		return;

	fd[*offset] = fd[0];
	if(compute_chunks_per_sect(fd + *offset, tracksize, sizecode,
				   &gap, /* gap. expressed in 1/256 bytes */
				   mask, tailsize) < 0) {
		/* not enough raw space for this arrangement */
		return;
	}

	for(i = firstSector(fd, tailsize);
	    i < lastSector(fd, tailsize);
	    i++) {
		fd[*offset] = fd[base];
		compute_sector_sequence(fd+*offset, i, gap);
		(*offset)++;
	}
}



int compute_all_sequences(struct params *fd, 
			  int tracksize,
			  int sizecode,
			  int gap,
			  int mask, 
			  int biggest_last)
{	
	int offset, i;
	
	compute_sizes(fd, sectors*512,sizecode);

	offset = 0;
	for(i=MAX_SIZECODE - 1 ; i >= 0; i--) {
		compute_all_sequences_for_size(fd, &offset, tracksize, 
					       sizecode, gap, mask, i);
		if(biggest_last && offset)
			break;
	}
	
	if(! offset){
		fprintf(stderr,
			"Not enough raw space on this disk for this format\n");
		exit(1);
	}
	return offset;
}



void compute_track0_sequence(struct params *fd)
{
	int i;
	int sectors;

	sectors= fd->nssect = fd->dsect;

	fd->length = fd->raw_capacity;
	fd->chunksize = 0x6c + 574;

	fd->need_init = 0;
	fd->sequence = SafeNewArray(fd->dsect,struct fparm2);

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
