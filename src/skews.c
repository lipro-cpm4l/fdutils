#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "fdutils.h"
#include "superformat.h"


/* computes the position at the end of this track */
static inline int calc_end_offset(struct params *f, int cur_pos, int *lskew)
{
	int sector_skew, byte_skew;

	if(cur_pos < f->min) {
		/* the current skew is smaller than the extend that this
		 * sequence can handle. Wait until its beginning */
		*lskew = 0;
		return f->min + f->length;
	}

	sector_skew = (cur_pos - f->min + f->chunksize - 1) / f->chunksize;
	byte_skew = f->min + sector_skew * f->chunksize;
	
	if(byte_skew >= f->max) {
		/* the current skew is larger than the extend that this
		 * sequence can handle. Wait until its next beginning */
		*lskew = 0;		
		return f->min + f->length + f->raw_capacity;
	}

	*lskew = sector_skew;
	return byte_skew + f->length;
}


static inline void pick_best(struct params *f, int n,
			     int *min, /* skew in bytes */
			     int *lskew, /* skew in sectors */
			     int *min_index)
{
	int i,new, t_lskew;
	int cur_pos = *min;

	for(i = 0 ; i < n;  i++) {
		new = calc_end_offset(f+i,  cur_pos, &t_lskew);
		if(i == 0 || new < *min) {
			*min = new;
			*lskew = t_lskew;
			*min_index = i;
		}
	}
}


/* calc_skews. Fill skew table for use in formatting */
int calc_skews(struct params *fd0, struct params *fd, int n)
{	
	int cylinder, head;
	struct params *f = NULL;
	int cur_skew, next_cylinder_skew;
	int ind, rots;

	/* Amount to advance skew considering head skew already added in */
	next_cylinder_skew = cylinder_skew - head_skew;
	cur_skew = absolute_skew;

	rots = 0;
	for (cylinder=begin_cylinder; cylinder <= end_cylinder; ++cylinder) {
		for (head=0; head < heads; ++head) {
			if (!head && !cylinder && use_2m)
				f = fd0;
			else
				f = fd;
			cur_skew = cur_skew % f->raw_capacity;			
			pick_best(f, n, &cur_skew, 
				  &lskews[cylinder][head], 
				  &findex[cylinder][head]);

			rots += cur_skew / f->raw_capacity;
			ind = findex[cylinder][head];
			if(lskews[cylinder][head] * fd[ind].chunksize >
			   fd->raw_capacity){
				fprintf(stderr,"Skew too big\n");
				abort();
			}
			cur_skew += head_skew;
		}
		cur_skew += next_cylinder_skew;
	}
	return 0;
} /* End calc_skews */
