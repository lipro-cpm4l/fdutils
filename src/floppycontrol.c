/*
 * floppycontrol.c
 *
 * This program illustrates the new FDGETEMSTRESH, FDSETMAXERRS, FDGETMAXERRS,
 * FDGETDRVTYP ioctl added by A. Knaff
 */

#include <stdio.h>
#include <sys/types.h>
#include <linux/fd.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "enh_options.h"

#ifndef FD_DISK_CHANGED
#define FD_DISK_CHANGED 0
#endif


int eioctl(int fd, int command,void * param, char *emsg)
{
  int r;
  if ((r=ioctl(fd,command,param))<0 )
    {
      perror(emsg);
      exit(1);
    }
  return r;
}

char *autodetect;
int reset_now;
struct floppy_drive_struct drivstat;
struct floppy_fdc_state fdcstat;
#ifdef FDWERRORGET
struct floppy_write_errors sta;
#endif
struct floppy_fdc_state fdcstat;
struct floppy_drive_params dpr;
#define max_error (dpr.max_errors)

int fd;

#define PRINT 0x1
#define SET_ERR 0x2
#define SET_EMSG 0x4
#define DO_FLUSH 0x8
#define DO_FMT_END 0x10
#define DO_DRVTYP 0x20
#define DO_EJECT 0x40
#define SET_DPR 0x80
#define PRINTSTATE 0x100
#define SET_AUTODETECT 0x200
#define POLLSTATE 0x400
#define SET_RESET 0x800
#define PRINTFDCSTATE 0x1000

#ifdef FDWERRORGET
#define CLRWERROR 0x2000
#define PRINTWERROR 0x4000
#endif

#ifndef FD_DEBUG
#define FD_DEBUG 2
#endif

struct enh_options optable[] = {
{ 'p', "print", 0, EO_TYPE_NONE, 0, PRINT, 0,
	  "print the current error thresholds and the drive type"},

{ 'a', "abort", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_ERR, 
	  (void *) &max_error.abort,
	  "set operation abortion threshold (if there are more errors than this threshold, the driver gives up to read/write the requested sector and reports an I/O error to the user program" },

{ 't', "readtrack", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_ERR, 
	  (void *) &max_error.read_track,
	  "set read track threshold (if there are less errors than this threshold, an entire track is read at once)" },

{ 'r', "recalibrate", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_ERR, 
	  (void *) &max_error.recal,
	  "set recalibration threshold (if there are more errors than this threshold, the driver recalibrates the drive, i.e. moves the head to track 0 and back, to make sure it really knows where the head is)" },

{ 'R', "reset", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_ERR, 
	  (void *) &max_error.reset,
	  "set reset threshold (if there are more errors than this threshold, the driver resets the floppy disk controller)" },

{ 'e', "reporting", 1, EO_TYPE_SHORT | EO_TYPE_DELAYED, 0, SET_ERR, 
	  (void *) &max_error.reporting,
	  "set error reporting threshold (if there are more errors than this threshold, error messages are printed to the console)" },

{ 'f', "flush", 0, EO_TYPE_NONE | EO_TYPE_DELAYED, 0, DO_FLUSH, 0,
	  "flush the floppy buffers" },

#ifdef FDEJECT
{ 'x', "eject", 0, EO_TYPE_NONE | EO_TYPE_DELAYED, 0, DO_EJECT, 0,
	  "eject the floppy disk" },
#endif

{ 'd', "drive", 1, EO_TYPE_FILE_OR_FD, 3, 0,
	  (void *) &fd,
	  "specify the target drive (default is /dev/fd0)" },

{ 'F', "formatend", 1, EO_TYPE_NONE | EO_TYPE_DELAYED, 0, DO_FMT_END, 0,
	  "emits an FDFMTEND ioctl. This is needed when, after interrupting a formatting program, the drive light stays on" },

{ 'T', "type", 0, EO_TYPE_NONE, 0, DO_DRVTYP, 0,
	  "prints the drive type" },

{'\0', "debug", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED, FD_DEBUG, SET_DPR,
	 (void *) &dpr.flags,
	 "switch debugging on" },

{'\0', "nodebug", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED, ~FD_DEBUG, SET_DPR,
	 (void *) &dpr.flags,
	 "switch debugging off" },

{'\0', "message", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED, FTD_MSG, SET_DPR,
	 (void *) &dpr.flags,
	 "switch autodetection and overrun messages on" },

{'\0', "nomessage", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED, ~FTD_MSG,SET_DPR,
	 (void *) &dpr.flags,
	 "switch autodetection and overrun messages off" },

#ifdef FD_SILENT_DCL_CLEAR
{'\0', "silent_dcl_clear", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED, 
	 FD_SILENT_DCL_CLEAR, SET_DPR,
	 (void *) &dpr.flags,
	 "switches on silent disk change status clear" },

{'\0', "noisy_dcl_clear", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED, 
	 ~FD_SILENT_DCL_CLEAR,SET_DPR,
	 (void *) &dpr.flags,
	 "switches off silent disk change status clear" },
#endif

{ 'c', "cmos", 1, EO_TYPE_BYTE | EO_TYPE_DELAYED, 0, SET_DPR,
	 (void *) &dpr.cmos,
	 "set cmos type" },

{ '\0', "hlt", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_DPR,
	 (void *) &dpr.hlt,
	 "set hlt" },

{ '\0', "hut", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_DPR,
	 (void *) &dpr.hut,
	 "set hut" },

{ '\0', "srt", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_DPR,
	 (void *) &dpr.srt,
	 "set srt" },

{ 'o', "spindown", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_DPR,
	  (void *) &dpr.spindown,
	  "set spindown time" },

{ 'u', "spinup", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_DPR,
	  (void *) &dpr.spinup,
	  "set spinup time" },

{ 's', "select_delay", 1, EO_TYPE_BYTE | EO_TYPE_DELAYED, 0, SET_DPR,
	  (void *) &dpr.select_delay,
	  "set drive select delay" },

{'\0', "rps", 1, EO_TYPE_BYTE | EO_TYPE_DELAYED, 0, SET_DPR,
	 (void *) &dpr.rps,
	 "sets rotations per second" },

{ 'O', "spindown_offset", 1, EO_TYPE_BYTE | EO_TYPE_DELAYED, 0, SET_DPR,
	  (void *) &dpr.spindown_offset,
	  "set spindown offset (decides in which position the disk stops)" },

{'\0', "cylinders", 1, EO_TYPE_BYTE | EO_TYPE_DELAYED, 0, SET_DPR,
	 (void *) &dpr.tracks,
	 "set maximal number of cylinders" },

{'\0', "tracks", 1, EO_TYPE_BYTE | EO_TYPE_DELAYED, 0, SET_DPR,
	 (void *) &dpr.tracks,
	 "obsolete (same as --cylinders)" },

{'\0', "timeout", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_DPR,
	 (void *) &dpr.timeout,
	 "set interrupt timeout" },

{ 'i', "interleave", 1, EO_TYPE_BYTE | EO_TYPE_DELAYED, 0, SET_DPR,
	 (void *) &dpr.interleave_sect,
	 "set interleave" },

{'A', "autodetect", 1, EO_TYPE_STRING, 0, SET_AUTODETECT,
	  (void *) &autodetect,
	  "set autodetection sequence (comma separated list of format indexes. The format index is the minor device number divided by four)" },

{ 'C', "checkfreq", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_DPR,
	  (void *) &dpr.checkfreq,
	  "set maximal disk change check interval" },

{ 'n', "native_format", 1, EO_TYPE_LONG | EO_TYPE_DELAYED, 0, SET_DPR,
	  (void *) &dpr.native_format,
	  "set the native format of this drive" },

{'\0', "resetnow", 1, EO_TYPE_LONG, 0, SET_RESET,
	 (void *) &reset_now,
	 "issue a reset command", },

{ 'P', "printstate", 0, EO_TYPE_NONE | EO_TYPE_DELAYED, 0, PRINTSTATE, 0,
	  "print internal per drive driver status"},

{ '\0', "printfdcstate", 0, EO_TYPE_NONE | EO_TYPE_DELAYED, 0, PRINTFDCSTATE, 0,
	  "print internal per fdc driver status"},

{ '\0', "pollstate", 0, EO_TYPE_NONE | EO_TYPE_DELAYED, 0, POLLSTATE, 0,
	  "polls internal driver status and prints it"},

#ifdef FDWERRORGET
{ '\0', "clrwerror", 0, EO_TYPE_NONE | EO_TYPE_DELAYED, 0, CLRWERROR, 0,
	  "clears write error structure"},

{ '\0', "printwerror", 0, EO_TYPE_NONE | EO_TYPE_DELAYED, 0, PRINTWERROR, 0,
	  "prints write error structure"},
#endif

#ifdef FD_BROKEN_DCL
{'\0', "broken_dcl", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED,FD_BROKEN_DCL,
	 SET_DPR,
	 (void *) &dpr.flags,
	 "work around broken disk change line" },
{'\0', "working_dcl", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED,~FD_BROKEN_DCL,
	 SET_DPR,
	 (void *) &dpr.flags,
	 "assume a working disk change line" },
#endif

#ifdef FD_INVERTED_DCL
{'\0', "inverted_dcl", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED,
	 FD_INVERTED_DCL,
	 SET_DPR,
	 (void *) &dpr.flags,
	 "assume an inverted disk change line" },
{'\0', "no_inverted_dcl", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED,
	 ~FD_INVERTED_DCL,
	 SET_DPR,
	 (void *) &dpr.flags,
	 "assume a non-inverted disk change line" },
#endif


{'\0', "messages", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED,FTD_MSG,
	 SET_DPR,
	 (void *) &dpr.flags,
	 "print informational messages" },
{'\0', "no_messages", 0, EO_TYPE_BITMASK_BYTE | EO_TYPE_DELAYED,~FTD_MSG,
	 SET_DPR,
	 (void *) &dpr.flags,
	 "don't print informational messages" },

{ '\0', 0 }
};

int main( int argc, char **argv)
{
	int ch;
	int mask=0;       
	char drivtyp[17];
	char *cont;
	int i;
		
	fd = -2;
	while((ch=getopt_enh(argc, argv, optable, 
			     0, &mask, "drive") ) != EOF ){
		if ( ch== '?' ){
			fprintf(stderr,"exiting\n");
			exit(1);
		}
		printf("unhandled option %d\n", ch);
		exit(1);
	}
	
	if ( fd == -1 )
		exit(0);

	if ( fd < 0 ){
		if ( optind < argc )
			fd = open(argv[optind], 3 | O_NDELAY);
		else
			fd = open("/dev/fd0", 3 | O_NDELAY);
		if ( fd < 0 ){
			perror("can't open floppy drive");
			print_usage(argv[0],optable,"");
			exit(1);
		}
	}
	
	if (mask & DO_FMT_END )
		eioctl(fd,FDFMTEND,0,"format end");

	if (mask & DO_FLUSH)
		eioctl(fd,FDFLUSH,0,"flush buffers");

#ifdef FDEJECT
	if (mask & DO_EJECT) {
		fsync(fd);
		eioctl(fd,FDEJECT,0,"eject floppy");
	}
#endif
	
	if (mask & (DO_DRVTYP ) )
		eioctl(fd,FDGETDRVTYP,(void *) drivtyp, "Get drive type");

	if ( mask & (PRINT | SET_DPR | SET_AUTODETECT ) )
		eioctl(fd,FDGETDRVPRM,(void *) &dpr, "Get drive parameters");
	else  if ( mask & SET_ERR )
		eioctl(fd,FDGETMAXERRS,(void *) &max_error, "Get max errors");
	
	if (mask & PRINT ){
		print_current_settings(optable, 
				       SET_ERR|SET_EMSG|SET_DPR);
		printf("autodetect seq.= ");
		for ( i=0; i<8; i++){
			if ( dpr.autodetect[i] == 0 )
				break;
			if ( i )
				putchar(',');
			printf("%1d", dpr.autodetect[i]);
			if ( dpr.read_track & ( 1 << i ) )
				putchar('t');
		}
		printf("\nflags: ");
		if(dpr.flags & 2 )
			printf("debug ");
		if(dpr.flags & FTD_MSG )
			printf("messages ");
#ifdef FD_BROKEN_DCL
		if(dpr.flags & FD_BROKEN_DCL )
			printf("broken_dcl");
#endif
#ifdef FD_SILENT_DCL_CLEAR
		if(dpr.flags & FD_SILENT_DCL_CLEAR)
			printf("silent_dcl_clear");
#endif
#ifdef FD_INVERTED_DCL
		if(dpr.flags & FD_INVERTED_DCL )
			printf("inverted_dcl");
#endif
		printf("\n");
	}

	if (mask & DO_DRVTYP )
		printf("%s\n", drivtyp);
	
	parse_delayed_options(optable, mask & (SET_ERR| SET_EMSG| SET_DPR));
	if ( mask & SET_AUTODETECT ){
		cont = autodetect;
		for ( i=0; i<8; i++){
			if ( *cont == '\0' )
				break;
			if ( *cont == ',' ){
				cont++;
				continue;
			}
			
			dpr.autodetect[i] = strtoul( cont, &cont, 0 );
			if ( *cont == 't' ){
				cont++;
				dpr.read_track |= 1 << i;
			} else
				dpr.read_track &= ~(1 << i);
			
			while ( *cont != ',' && *cont != '\0' )
				cont++;
			if ( *cont == ',' )
				cont++;
		}
		if ( i < 8 )
			dpr.autodetect[i] = 0;    
	}

	if (mask & SET_RESET)
		eioctl(fd, FDRESET, (void *)reset_now, "reset");

	if (mask & (PRINTSTATE | POLLSTATE) ){
		if ( mask & POLLSTATE )
			eioctl( fd, FDPOLLDRVSTAT, &drivstat,"get drive state");
		else
			eioctl( fd, FDGETDRVSTAT , &drivstat,"get drive state");

#ifndef FD_DCL_SEEN
# define FD_DCL_SEEN 0x40
#endif
		printf("%s %s %s %s %s %s\n", 
		       drivstat.flags & FD_VERIFY ? "verify" : "",
		       drivstat.flags & FD_DISK_NEWCHANGE ? "newchange" : "",
		       drivstat.flags & FD_NEED_TWADDLE ? "need_twaddle" : "",
		       drivstat.flags & FD_DISK_CHANGED ? "disk_changed" : "",
		       drivstat.flags & FD_DISK_WRITABLE ?"disk_writable" : "",
		       drivstat.flags & FD_DCL_SEEN ?"dcl_seen" : "");
		printf("spinup=		%ld\n", drivstat.spinup_date);
		printf("select=		%ld\n", drivstat.select_date);
		printf("first_read=	%ld\n", drivstat.first_read_date);
		printf("probed_fmt=    	%d\n", drivstat.probed_format);
		printf("cylinder=      	%d\n", drivstat.track);
		printf("maxblock=      	%d\n", drivstat.maxblock);
		printf("maxcylinder=	%d\n", drivstat.maxtrack);
		printf("generation=    	%d\n", drivstat.generation);
		printf("keep data=	%d\n", drivstat.keep_data);
		printf("refs=		%d\n", drivstat.fd_ref);
		printf("device=		%d\n", drivstat.fd_device);
		printf("last_checked=  	%ld\n", drivstat.last_checked);
	}


	if ( mask & PRINTFDCSTATE ){
		eioctl( fd, FDGETFDCSTAT , &fdcstat,"get fdc state");
		printf("spec1=%x\n", fdcstat.spec1);
		printf("spec2=%x\n", fdcstat.spec2);
		printf("rate=%x\n", fdcstat.dtr);
		printf("rawcmd=%x\n", fdcstat.rawcmd);
		printf("dor=%x\n", fdcstat.dor);
		printf("version=%x\n", fdcstat.version);
		printf("reset=%x\n", fdcstat.reset);
		printf("need_configure=%x\n", fdcstat.need_configure);
		printf("has_fifo=%x\n", fdcstat.has_fifo);
		printf("perp_mode=%x\n", fdcstat.perp_mode);
		printf("address=%x\n", (unsigned int) fdcstat.address);
	}

#ifdef FDWERRORGET
	if (mask & PRINTWERROR){
		eioctl( fd, FDWERRORGET , &sta,"get write error state");
		printf("write_errors %u  badness %u\n",
		       sta.write_errors, sta.badness);
		printf("first_error_sector %lu  first_error_generation %d\n",
		       sta.first_error_sector, sta.first_error_generation);
		printf("last_error_sector  %lu  last_error_generation  %d\n",
		       sta.last_error_sector, sta.last_error_generation);
	}
	if (mask & CLRWERROR)
		eioctl( fd, FDWERRORCLR , 0,"clear write error state");
#endif

	if (mask & ( SET_DPR | SET_AUTODETECT ))
		eioctl(fd,FDSETDRVPRM, (void *) &dpr, "Set drive parameters");
	else if (mask & SET_ERR )
		eioctl(fd,FDSETMAXERRS, (void *) &max_error, "Set max errors");

	exit(0);
}

