#include <stdio.h>
#include "printfdprm.h"


void print_params(drivedesc_t *drivedesc,
		  struct floppy_struct *ft,
		  level_t level, int cpm,
		  void (*print)(char *,int))
{
	int ssize, r, perp_ref, need_dtr, need_fm, need_tpi;
	ff_t formFactor;

	if(drivedesc)
		formFactor = drivedesc->type.ff;
	else
		formFactor = FF_UNKNOWN;

	need_dtr = 0;
	need_fm = 0;
	need_tpi = 0;
	
	switch(ft->head) {
		case 1:
			print("SS",0);
			break;
		case 2:
			print("DS",0);
			break;
	}
	perp_ref = 0;
	switch(r = ft->rate & 0x83) {
		case 0:			
			print("HD",0);
			break;
		case 0x80:
			need_tpi = need_fm = need_dtr = 1;
			print("DD", 0); /* 8 " DD */
			break;
		case 1:
		case 2:
			switch(formFactor) {
				case FF_35:
					if(r == 1)
						/* modified double-density
						 * format with slightly higher
						 * DTR */
						print("QD",0);
					else
						print("DD",0);
					break;
				case FF_525:
					if(ft->track >= 60)
						print("QD",0);
					else
						print("DD",0);
					break;
				default:
					/* we do not not the drive type.
					 * Guesswork */
					if(ft->track <= 60)
						/* this covers any 40 track
						 * formats */
						print("DD",0);
					else if(r == 1)
						/* this covers the special
						 * density 3 1/2 format, as
						 * well as the quad density 
						 * 5 1/4 format */
						print("QD",0);
					else
						/* this leaves norma 3 1/2 DD */
						print("DD",0);
					break;
			}
			break;
		case 3:
			perp_ref = 0x40;
			print("ED",0);
			break;
		default:
			need_tpi = need_fm = need_dtr = 1;
			print("SD",0);
			break;
	}

	switch(formFactor) {
		/* TPI is only meaningful for 5 1/4 drives.  As other
		 * drives have different physical sizes, the numbers of
		 * 48 or 96 would be inappropriate anyways: use stretch in
		 * that case */		   
		case FF_525:
			if(level >= LEV_ALL) {
				if(ft->track <= 60)
					print("tpi=48",0);
				else
					print("tpi=96",0);
			}
			break;
		case FF_UNKNOWN:
			if(level >= LEV_ALL && ft->track <= 60)
				print("tpi=48",0);
			break;
		default:
			if(level >= LEV_ALL || ft->stretch)
				print("stretch=%d", ft->stretch);
			break;			
	}

	if(level >= LEV_ALL || ft->size != ft->sect * ft->head * ft->track)
		print("size=%d", ft->size);

	ssize = (((ft->rate & 0x38) >> 3) + 2) % 8;
	if(ssize == 2)
		print("sect=%d", ft->sect);
	else {
		if( (ft->sect << 2) % (1 << ssize) == 0)
			print("sect=%d", ft->sect << 2 >> ssize);
		else {
			if(ft->sect % 2)
				print("tracksize=%db", ft->sect);
			else
				print("tracksize=%dKB", ft->sect / 2);
				
		}
	}

	if(level >= LEV_EXPL || need_dtr)
		print("dtr=%d", ft->rate & 3);

	if(level >= LEV_EXPL || perp_ref != (ft->rate & 0x40))
		print("perp=%d", (ft->rate & 0x40) >> 6);

	if(level >= LEV_EXPL || need_fm)
		print("fm=%d", (ft->rate & 0x80) >> 7);

	if(level >= LEV_EXPL || (ft->track != 80 && ft->track != 40))
		print("cyl=%d", ft->track);

	if(ft->stretch & FD_SWAPSIDES)
		print("swapsides",0);			

	if(ft->stretch & FD_ZEROBASED)
		print("zerobased",0);			


	if(ft->rate & FD_2M)
		print("2M",0);
	
	if(level < LEV_ALL &&
	   ft->sect * 4 <  (2 << ssize) && ft->sect * 4 % (1 << ssize))
		print("mss", 0);
	else {
		if(ssize < 2)
			print("ssize=%d", 128 << ssize);
		else if(ssize > 2)
			print("ssize=%dKB", 1 << (ssize-3));
		else if(level >= LEV_ALL)
			print("ssize=512", 0);
	}

	/* useless stuff */
	if(level >= LEV_MOST) {		
		print("gap=0x%02x", (unsigned char) ft->gap);
		print("fmt_gap=0x%02x", (unsigned char) ft->fmt_gap);
	}

	if(level >= LEV_ALL)
		print("spec1=0x%02x", (unsigned char) ft->spec1);
	printf("\n");
}
