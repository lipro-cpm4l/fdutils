#ifndef DRIVEPRM_H
#define DRIVEPRM_H

#define DRIVEPRMFILE SYSCONFDIR "/driveprm"

#include <linux/fd.h>
#include <sys/stat.h>

/* different densities */
typedef enum
{
	DENS_UNKNOWN, DENS_SD, DENS_DD, DENS_QD, DENS_HD, DENS_ED
} density_t;


/* various drive form factors */
typedef enum
{
	FF_UNKNOWN, FF_35, FF_525, FF_8 
} ff_t;

typedef struct {
	int cmos;
	ff_t ff;
	density_t max_density; /* maximal supported density */
	int tpi;
	int rpm;
	int deviation;
} drivetypedesc_t;  

/* drive descriptor */
typedef struct {
	drivetypedesc_t type;
	int drivenum; /* the drive number [0-7] */
	struct stat buf;
	struct floppy_drive_params drvprm;
	int mask;
} drivedesc_t;

int parse_driveprm(int fd, drivedesc_t *drive);

typedef enum {
	FE__UNKNOWN,
	FE__CMOS,
	FE__TPI,
	FE__FF,
	FE__RPM,
	FE__DEVIATION,
	FE__DENSITY,
} drive_field_t;
#endif
