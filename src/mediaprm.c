#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/fd.h>
#include "parse.h"
#include "mediaprmP.h"
#include "driveprm.h"
#include "mediaprm.h"

typedef enum
{
	FE_UNKNOWN,
	FE_SIZE,
	FE_SECT,
	FE_VSECT,
	FE_HEAD,
	FE_CYL,

	/* the stretch byte */
	FE_TPI,
	FE_STRETCH,
	FE_SWAPSIDES,
	FE_ZEROBASED,

	FE_GAP,

	FE_FM,
	FE_PERP,
	FE_SSIZE,
	FE_2M,
	FE_DTR,

	FE_SPEC1, /* spec1, obsolete */
	FE_FMT_GAP,

	FE_DENSITY,
} field_t;

static int SIZE, SECT, VSECT, HEAD, CYL, TPI, STRETCH, SWAPSIDES, ZEROBASED;
static int GAP, FM;
static int PERP, SSIZE, _2M, DTR, SPEC1, FMT_GAP, DENSITY;
static int ssize;

#define F_SIZE FE_SIZE,&SIZE
#define F_SECT FE_SECT,&SECT
#define F_VSECT FE_VSECT,&VSECT
#define F_HEAD FE_HEAD,&HEAD
#define F_CYL FE_CYL,&CYL

#define F_TPI FE_TPI,&TPI
#define F_STRETCH FE_STRETCH,&STRETCH
#define F_SWAPSIDES FE_SWAPSIDES,&SWAPSIDES
#define F_ZEROBASED FE_ZEROBASED,&ZEROBASED

#define F_GAP FE_GAP,&GAP

#define F_FM FE_FM,&FM
#define F_PERP FE_PERP,&PERP
#define F_SSIZE FE_SSIZE,&SSIZE
#define F_2M FE_2M,&_2M
#define F_DTR FE_DTR,&DTR

#define F_SPEC1 FE_SPEC1,&SPEC1
#define F_FMT_GAP FE_FMT_GAP,&FMT_GAP

#define F_DENSITY FE_DENSITY,&DENSITY

static int mask;

static keyword_t ids[]= {
	{ "size", F_SIZE, 0 },
	{ "sect", F_SECT, 0},
	{ "tracksize", F_VSECT, 0},
	{ "head", F_HEAD, 0},
	{ "ss",   F_HEAD, 1},
	{ "ds",   F_HEAD, 2},
	{ "cyl",  F_CYL, 0},

	{ "tpi", F_TPI, 0 },
	{ "stretch", F_STRETCH, 0 },

	{ "swapsides", F_SWAPSIDES, 1},
	{ "zerobased", F_ZEROBASED, 1},
	{ "zero-based", F_ZEROBASED, 1},

	{ "gap", F_GAP, 0},

	{ "fm", F_FM, 1},
	{ "perp", F_PERP, 0},
	{ "ssize", F_SSIZE, 0},
	{ "2m", F_2M, 1},
	{ "dtr", F_DTR, 0},

	{ "mss", F_SSIZE, 16384 },
	{ "sd", F_DENSITY, DENS_SD },
	{ "dd", F_DENSITY, DENS_DD },
	{ "qd", F_DENSITY, DENS_QD },
	{ "hd", F_DENSITY, DENS_HD },
	{ "ed", F_DENSITY, DENS_ED },
	
	{ "spec1", F_SPEC1, 0},
	{ "fmt_gap", F_FMT_GAP, 0}
};


struct
{
	int dtr; /* dtr, as it would be in a 3 1/2 or DD drive */
	int fm; /* fm mode */
	int perp; /* perp mode */
	int tpi;
	int capacity[4]; /* capacity per track */
} dens_tab[] = {
	{ 0, 0, 0, 0}, /* none */
	{2, 1, 0, 48,  { 2500, 2500, 2500, 2500} }, /* SD */
	{2, 0, 0, 48,  { 6250, 6250, 6250, 6250} }, /* DD */
	{1, 0, 0, 96,  { 6250, 7500, 6250,    0} }, /* QD */
	{0, 0, 0, 96,  {12500,12500,10416,    0} }, /* HD */
	{3, 0, 1, 96,  {25000,25000,    0,    0} }  /* ED */
};

int gap[4] = { 0x1b, 0x23, 0x2a, 0x1b };

static inline void set_field(field_t slot, int *var, int value)
{
	if( !(mask & (1 << slot)))
		*var = value;
}

static void compute_params(drivedesc_t *drvprm,
			   struct floppy_struct *medprm)
{
	int r, capacity, mysize;
	int header_size, fmt_gap;

	set_field(F_DENSITY, drvprm->type.max_density);
	
	r = dens_tab[DENSITY].dtr;
	if(r == 2 && drvprm->type.rpm == 360)
		r = 1;

	capacity = dens_tab[DENSITY].capacity[drvprm->type.ff];

	set_field(F_DTR, r);

	set_field(F_GAP, gap[DTR]);
	
	set_field(F_FM, dens_tab[DENSITY].fm);
	set_field(F_PERP, dens_tab[DENSITY].perp);

	set_field(F_HEAD, 2);

	/* 5 1/4 drives have two different cylinder densities */
	if(drvprm->type.ff == FF_525) {
		if(DENSITY == DENS_DD)
			/* double density disks are 48 TPI */
			set_field(F_TPI, 48);
		else
			set_field(F_TPI, 96);

		if(mask & (1 << FE_CYL)) {
			/* we know the number of cylinders, try to infer
			 * cylinder density from there */
			if(CYL < 50)
				set_field(F_TPI, 48);
			if(CYL > 70)
				set_field(F_TPI, 96);
		}

		if(drvprm->type.tpi == 96 && TPI == 48)
			set_field(F_STRETCH, 1);
		else
			set_field(F_STRETCH, 0);
		
		/* if, on the other hand, we know TPI, but not the number of
		 * cylinders, infer in the other direction */
		if(TPI == 48)
			set_field(F_CYL, 40);
		else
			set_field(F_CYL, 80);
	} else {
		set_field(F_STRETCH, 0);
		set_field(F_CYL, 80);
	}

	if(PERP)
		header_size = 81;
	else
		header_size = 62;

	switch(capacity) {
		case 25000:
			set_field(F_SECT, 36);
			break;
		case 12500:
			set_field(F_SECT, 18);
			break;
		case 10416:
			set_field(F_SECT, 15);
			break;
		case 6250:
			set_field(F_SECT, 9);
			break;
		default:
			set_field(F_SECT, capacity / (SSIZE + header_size + 1));
	}

	set_field(F_SSIZE, 512);
	set_field(F_VSECT, SECT * SSIZE);
	set_field(F_SIZE, HEAD * CYL * VSECT / 512);	
		
	if(mask & ( 1 << FE_SSIZE) ) {
		mysize = 128;
		for(ssize = 0; ssize < 8 ; ssize++) {
			if(mysize == SSIZE)
				break;
			mysize += mysize;
		}
		if(ssize == 8) {
			fprintf(stderr,"Bad sector size\n");
			exit(1);
		}
		ssize = (ssize + 6) % 8;
	} else
		ssize = 0;
	
	if(VSECT && VSECT >= SSIZE) {
		fmt_gap = (capacity * 199 / 200 / (VSECT / SSIZE)) - 
			SSIZE - header_size;
		if(fmt_gap < 1)
			fmt_gap = 1;
		if(fmt_gap > 0x6c)
			fmt_gap = 0x6c;
		set_field(F_FMT_GAP, fmt_gap);
	}
	set_field(F_2M,0);

	medprm->size = SIZE;
	medprm->sect = VSECT / 512;
	medprm->head = HEAD;
	medprm->track = CYL;
	medprm->stretch = STRETCH | (SWAPSIDES << 1) | (ZEROBASED << 2);
	medprm->gap = GAP;
	medprm->rate = (FM<<7) | (PERP<<6) | (ssize<<3) | (_2M<<2) | DTR;
	medprm->spec1 = SPEC1;
	medprm->fmt_gap = FMT_GAP;
}


/* ========================================= *
 * Routines called by other parts of fdutils *
 * ========================================= */


static int parse_indirect(int argc, char **argv,
		  drivedesc_t *drvprm,
		  struct floppy_struct *medprm)
{
	int found;

	if(argc != 1)
		return 2; /* more than one parameter */
	mediaprmin = fopen(MEDIAPRMFILE, "r");
	if(!mediaprmin)
		return 2; /* no file */

	zero_all(ids, &mask);

	found = 0;
	mediaprmlex(argv[0], ids, sizeof(ids)/sizeof(ids[0]), &mask, &found);
	fclose(mediaprmin);
	if(!found)
		return 1;
	compute_params(drvprm, medprm);
	return 0;
}

static int parse_direct(int argc, char **argv,
			drivedesc_t *drvprm,
			struct floppy_struct *medprm)
{
	int i,r;
	mask = 0;
	zero_all(ids, &mask);
	for(i=0; i<argc; i++) {
		r=set_int(argv[i], ids, &mask);
		if(r)
			return r;
	}
	compute_params(drvprm, medprm);
	return 0;
}


/* ========================================= */
/* Testing                                   */
/* ========================================= */


int parse_mediaprm(int argc, char **argv,
		   drivedesc_t *drvprm,
		   struct floppy_struct *medprm)		  
{
	int r;
	
	return ((r=parse_direct(argc, argv, drvprm, medprm) &&
		 (r=parse_indirect(argc, argv, drvprm, medprm))));
}
