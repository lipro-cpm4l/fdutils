#include <sys/types.h>
#ifdef HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/fd.h>
#include <sys/ioctl.h>
#include <linux/major.h>
#include "parse.h"
#include "driveprm.h"
#include "driveprmP.h"

typedef drive_field_t field_t;

static int TPI, RPM, DEVIATION, FF, DENSITY, CMOS;

#define F_CMOS FE__CMOS,&CMOS
#define F_TPI FE__TPI,&TPI
#define F_FF FE__FF,&FF
#define F_RPM FE__RPM,&RPM
#define F_DEVIATION FE__DEVIATION,&DEVIATION
#define F_DENSITY FE__DENSITY,&DENSITY

drivetypedesc_t cmos_types[]= {
	{ 0, FF_UNKNOWN, DENS_UNKNOWN, 0, 0 },
	{ 1, FF_525, DENS_DD, 48, 300, 0 },
	{ 2, FF_525, DENS_HD, 96, 360, 0 },
	{ 3, FF_35, DENS_DD, 0, 300, 0 },
	{ 4, FF_35, DENS_HD, 0, 300, 0 },
	{ 5, FF_35, DENS_ED, 0, 300, 0 },
	{ 6, FF_35, DENS_ED, 0, 300, 0 } 
};

static keyword_t ids[]= {
	{ "cmos", F_CMOS, 0},
	{ "tpi", F_TPI, 0 },

	{ "rpm", F_RPM, 0},
	{ "deviation", F_DEVIATION, 0},

	{ "sd", F_DENSITY, DENS_SD },
	{ "dd", F_DENSITY, DENS_DD },
	{ "qd", F_DENSITY, DENS_QD },
	{ "hd", F_DENSITY, DENS_HD },
	{ "ed", F_DENSITY, DENS_ED },
	
	{ "5.25", F_FF, FF_525 },
	{ "3.5", F_FF, FF_35 },
	{ "8", F_FF, FF_8 }
};

static int mask;

#define ISSET(x) ((mask) & ( 1 << FE__##x))

static void compute_params(drivedesc_t *drive)
{	
	/* initialize out array */
	drive->type.cmos = 0;

	drive->type.ff = FF_UNKNOWN;
	drive->type.max_density = DENS_UNKNOWN;

	drive->type.tpi = 0;
	drive->type.rpm = 0;
	drive->type.deviation = 0;

	if(!ISSET(CMOS)) {
		if(ISSET(FF) && ISSET(DENSITY)) {
			switch(DENSITY) {
				case DENS_DD:
					if(FF == FF_525)
						CMOS = 1;
					else
						CMOS = 3;
					break;
				case DENS_HD:
					switch(FF) {
						case FF_525:
							CMOS = 2;
							break;
						case FF_35:
							CMOS = 4;
							break;
					}
					break;
				case DENS_ED:
					switch(FF) {
						case FF_35:
							CMOS = 6;
							break;
					}
					break;
			}
		} else {
			CMOS = drive->drvprm.cmos;
			if (CMOS < 1 || CMOS > 6)
				CMOS = 4;
		}
	}

	if(CMOS) {
		if(CMOS > 6 || CMOS < 1) {
			fprintf(stderr, "Bad cmos code %d\n", CMOS);
			exit(1);
		}
		drive->type = cmos_types[CMOS];
	}

	if(ISSET(RPM))
		drive->type.rpm = RPM;

	if(ISSET(TPI))
		drive->type.tpi = TPI;

	if(ISSET(FF))
		drive->type.ff = FF;
		
	if(ISSET(DENSITY))
		drive->type.max_density = DENSITY;

	if(ISSET(RPM))
		drive->type.rpm = RPM;

	if(ISSET(DEVIATION))
		drive->type.deviation = DEVIATION;

	drive->mask = mask;
}

static int getdrivenum(int fd, struct stat *buf)
{
	int num;

	if (fstat (fd, buf) < 0) {
		perror("Can't fstat drive");
		exit(1);
	}

	if (!S_ISBLK(buf->st_mode) || 
	    major(buf->st_rdev) != FLOPPY_MAJOR) {
		fprintf(stderr,"Not a floppy drive\n");
		exit(1);
	}
	
	num = minor( buf->st_rdev );
	return (num & 3) + ((num & 0x80) >> 5);
}


/* ========================================= *
 * Routines called by other parts of fdutils *
 * ========================================= */

int parse_driveprm(int fd, drivedesc_t *drive)
{
	int found;

	drive->drivenum = getdrivenum(fd, &drive->buf);
	if(drive->drivenum < 0)
		return -1;

	ioctl(fd, FDRESET, FD_RESET_IF_RAWCMD);
	if (ioctl(fd, FDGETDRVPRM, & drive->drvprm ) < 0 ){
		perror("get drive characteristics");
		exit(1);
	}

	zero_all(ids, &mask);
	driveprmin = fopen(DRIVEPRMFILE, "r");

	if(driveprmin) {
		/* if the file doesn't exist, infer all info from the cmos type
		 * stored in the floppy driver */
		found = 0;
		driveprmlex(drive->drivenum, ids, sizeof(ids)/sizeof(ids[0]), 
			    &mask, &found);
		if(!found)
			zero_all(ids, &mask);
		fclose(driveprmin);
	}
	compute_params(drive);
	return 0;
}
