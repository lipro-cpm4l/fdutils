#ifndef MEDIAPRM_H
#define MEDIAPRM_H
#include "driveprm.h"

#define MEDIAPRMFILE SYSCONFDIR "/mediaprm"

int parse_mediaprm(int argc, char **argv,
		   drivedesc_t *drvprm,
		   struct floppy_struct *medprm);
#endif
