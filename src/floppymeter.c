#include <sys/types.h>
#ifdef HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fd.h>
#include <linux/fdreg.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/major.h>
#include <getopt.h>
#include "enh_options.h"
#include "fdutils.h"
#include "driveprm.h"

#define NON_INTERACTIVE 1

long long gettime(void)
{
  struct timeval tv;
  int ret;

  ret = gettimeofday(&tv, 0);
  if ( ret < 0 ){
    perror("get time of day\n");
    exit(1);
  }

  return ( (long long) tv.tv_usec + ( (long long) tv.tv_sec ) * 1000000LL);
}

int dtr[4] = { 500000, 300000, 250000, 1000000 };

struct {
	int time_per_rot; /* nominal time per rotation */
	int sp_offset;
	int rate[3];
} cmos_table[]= {
  {      0, 0, { 0x100, 0x100, 0x100} }, /* no drive */
  { 200000, 0, { 0x002, 0x100, 0x100} }, /* 5 1/4 DD */
  { 200000, 0, { 0x002, 0x100, 0x100} }, /* 3 1/2 DD */
  { 166666, 0, { 0x001, 0x000, 0x100} }, /* 5 1/4 HD */
  { 200000, 0, { 0x002, 0x000, 0x100} }, /* 3 1/2 HD */
  { 200000, 0, { 0x002, 0x000, 0x043} }, /* 3 1/2 ED */
  { 200000, 0, { 0x002, 0x000, 0x043} }  /* 3 1/2 ED */
};


struct reg {
	long long time;
	int rot;
};


int sliding_avg(struct reg *reg, int n)
{
	int i;
	long long sum_x, sum_y, sum_xy, sum_x2, sum2_x;

	sum_x = sum_y = sum_xy = sum_x2 = 0;
	for(i=0; i<n; i++) {
		sum_x += reg[i].rot;
		sum_y += reg[i].time;
		sum_xy += reg[i].rot * reg[i].time;
		sum_x2 += reg[i].rot * reg[i].rot;
	}	
	sum2_x = sum_x * sum_x;
	return (n*sum_xy - sum_x * sum_y) / (n*sum_x2 - sum2_x);
}

int main(int argc, char **argv)
{
	int n;
	long long shall_cap;
	int fd=-2;
	long long sum_x, sum_y, sum_xy, sum_x2, sum2_x;
	long long avg, base_time, last_time, time, shall_time, my_dtr;
	struct floppy_drive_params dpr;
	char c;
	int ch;
	int rot;
	int missed;
	int j;
	int dn,rate, cap;
	struct stat buf;
	char *name;
	int cycles=1000;
	int warmup=100;
	int mask=0;
	int density=3;
	int window=10;
	int cylinder=0;

	struct enh_options optable[] = {
		{ 'C', "cycles", 1, EO_TYPE_LONG , 0, 0, 
		  (void *) &cycles,
		  "run during n cycles" },

		
		{ 'f', "force", 0, EO_TYPE_NONE, 0, NON_INTERACTIVE, 0,
		  "don't ask for confirmation" },

		{ 'w', "warmup", 1, EO_TYPE_LONG, 0, 0, (void *) &warmup,
		  "number of warm-up rotations" },

		{ 'W', "window", 1, EO_TYPE_LONG, 0, 0, (void *) &window,
		  "window for sliding average" },

		{ 'c', "cylinder", 1, EO_TYPE_LONG, 0, 0, (void *) &cylinder,
		  "cylinder used for test" },


		{ '\0', "dd", 0, EO_TYPE_LONG, 0, 0, (void *) &density,
		  "perform test on a double density disk" },

		{ '\0', "hd", 0, EO_TYPE_LONG, 1, 0, (void *) &density,
		  "perform test on a high density disk" },

		{ '\0', "ed", 0, EO_TYPE_LONG, 2, 0, (void *) &density,
		  "perform test on an extra density disk" },

		{ '\0', 0 }
	};

	struct reg *reg;

	while((ch=getopt_enh(argc, argv, optable, 
			     0, &mask, "drive") ) != EOF ){
		if ( ch== '?' ){
			fprintf(stderr,"exiting\n");
			exit(1);
		}
		printf("unhandled option %d\n", ch);
		exit(1);
	}


	if ( optind < argc )
		name = argv[optind];
	else
		name = "/dev/fd0";
	fd = open(name, 3 | O_NDELAY);
	if ( fd < 0 ){
		perror("can't open floppy drive");
		print_usage(argv[0],optable,"");
		exit(1);
	}

	if(!(mask & NON_INTERACTIVE)) {
		fprintf(stderr,
			"Warning: all data contained on the floppy disk will be lost. Continue?\n");
		c=getchar();
		if(c != 'y' && c != 'Y')
			exit(1);
	}

	if (fstat (fd, &buf) < 0) {
		perror("fstat");
		exit(1);
	}
	if (!S_ISBLK(buf.st_mode) || major(buf.st_rdev) != FLOPPY_MAJOR) {
		fprintf(stderr,"%s is not a floppy drive\n", name);
		exit(1);
	}
	dn = minor( buf.st_rdev );
	dn = (dn & 3) + ((dn & 0x80) >> 5);

	if(ioctl(fd, FDGETDRVPRM, &dpr) < 0) {
		perror("get drive parameter");
		exit(1);
	}

	if(dpr.cmos < 1 || dpr.cmos > 6) {
		fprintf(stderr, "Bad cmos type\n");
		exit(1);
	}

	if(density == 3) {
		do {
			density--;
			rate = cmos_table[(int) dpr.cmos].rate[density];
		} while(rate >= 256);
	} else
		rate = cmos_table[(int) dpr.cmos].rate[density];
	if(rate >= 256) {
		fprintf(stderr,"density not supported by this drive\n");
		exit(1);
	}

	/* measure its capacity */
	measure_raw_capacity(fd, dn, rate, 
			     cylinder, warmup, !(mask & NON_INTERACTIVE));

	readid(fd, dn, rate, cylinder);
	base_time = last_time = gettime();
	missed = rot = 0;
	avg = cmos_table[(int)dpr.cmos].time_per_rot;
	reg = (struct reg *) calloc(window, sizeof(struct reg));
	for(j=0; j<window; j++) {
		reg[j].rot = 0;
		reg[j].time = base_time;
	}
	j=0;
	n=1;
	if(!(mask & NON_INTERACTIVE)) {
		fprintf(stderr,
			" _____________________________________________\n");
		fprintf(stderr,
			"|rotations  |average    |sliding    |missed   |\n");
		fprintf(stderr,
			"|since start|time per   |average of |rotations|\n");
		fprintf(stderr,
			"|of test    |rotation   |the last   |         |\n");
		fprintf(stderr,
			"|           |since start|%-8d   |         |\n",window);
		fprintf(stderr,
			"|           |of test    |rotations  |         |\n");
		fprintf(stderr,
			"|===========|===========|===========|=========|\n");
	}

	sum_x = sum_y = sum_xy = sum_x2 = 0;
	sum_y = base_time;
	while(rot < cycles) {
		readid(fd, dn, rate, cylinder);
		rot++;
		time = gettime();
		if(time - last_time > avg * 3 / 2) {
			/* if it takes unusually long, we may have missed one rotation */
			rot += (time - last_time - avg / 2) / avg;
			missed += (time - last_time - avg / 2) / avg;
		}

		last_time = time;
		reg[j].time = time;
		reg[j].rot = rot;

		sum_x += rot;
		sum_y += time;
		sum_xy += rot * time;
		sum_x2 += rot * rot;
		sum2_x = sum_x * sum_x;
		n++;

		avg = ((n*sum_xy - sum_x*sum_y) / (n*sum_x2 - sum2_x));
		if(!(mask & NON_INTERACTIVE)){
			fprintf(stderr,"|%6d     |%8d   |%8d   |%3d      |\r", 
				rot, (int)avg, sliding_avg(reg,window), missed);
			fflush(stderr);
		}
		j++;
		if(j>=window)
			j=0;
	}	
	if(!(mask & NON_INTERACTIVE)) {
		fprintf(stderr,"\n");
		fprintf(stderr,
			"|___________|___________|___________|_________|\n");
	}

	cap = measure_raw_capacity(fd, dn, rate, 
				   cylinder, 0, !(mask & NON_INTERACTIVE));

	shall_time = cmos_table[(int)dpr.cmos].time_per_rot;
	shall_cap = dtr[rate] * 2 * shall_time / 1000000LL;

	printf("\n");
	printf("capacity=%d half bits (should be %d half bits)\n", cap,
	       (int) shall_cap);
	printf("time_per_rotation=%d microseconds (should be %d)\n", (int) avg,
	       (int) shall_time);
	my_dtr = ((long long)cap) * 500000 / avg;
	printf("data transfer rate=%d bits per second (should be %d)\n", 
	       (int) my_dtr, dtr[rate]);
	printf("\n");
	printf("deviation on capacity: %+d ppm\n", 
	       (int) ((cap - shall_cap) * 1000000 / shall_cap));
	printf("deviation on time_per_rotation: %+d ppm\n",
	       (int) ((avg - shall_time) * 1000000 / shall_time));
	printf("deviation on data transfer rate: %+d ppm\n", 
	       (int) ((my_dtr - dtr[rate]) * 1000000 / dtr[rate]));

	printf("\nInsert the following line to your " DRIVEPRMFILE " file:\n");
	printf("drive%d: deviation=%d\n\n", 
	       dn, (int) ((cap - shall_cap) * 1000000 / shall_cap ));

#if 0
	margin = (shall_cap - cap + 15)/10;
	if(margin > 0)
		printf("recommended margin: %d\n", margin);
#endif
	exit(0);
}
