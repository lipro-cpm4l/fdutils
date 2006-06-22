#ifndef PRINTFDPRM_H
#define PRINTFDPRM_H

#include "driveprm.h"

typedef enum {
	LEV_NONE,
	LEV_EXPL,
	LEV_MOST,
	LEV_ALL,
	LEV_OLD
} level_t;

void print_params(drivedesc_t *drivedesc,
		  struct floppy_struct *ft,
		  level_t level, int cpm,
		  void (*print)(char *,int));
#endif
