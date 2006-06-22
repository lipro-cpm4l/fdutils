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

struct lookup_table
{
	char *name;
	int cmd;
	int flags;
} tabl[]= {
	{ "read",		FD_READ,	       	FD_RAW_READ | FD_RAW_INTR },
	{ "read_track",      	0x42,		       	FD_RAW_READ | FD_RAW_INTR },
	{ "write",		FD_WRITE,      		FD_RAW_WRITE | FD_RAW_INTR },
	{ "intr",		0,			FD_RAW_INTR },
#ifdef FD_RAW_NO_MOTOR
	{ "no_motor",		0,			FD_RAW_NO_MOTOR },
	{ "no_motor_after",  	0,			FD_RAW_NO_MOTOR_AFTER },
#endif
	{ "sensei",		FD_SENSEI,	     	0 },
	{ "disk",		0,			FD_RAW_NEED_DISK },
	{ "need_seek",       	0,			FD_RAW_NEED_SEEK },
	{ "spin",		0,			FD_RAW_SPIN },
	{ "sense",		FD_GETSTATUS,		0},
	{ "recalibrate",	FD_RECALIBRATE,		FD_RAW_INTR },
	{ "seek",		FD_SEEK,		FD_RAW_INTR },
#ifdef FD_RSEEK_OUT
	{ "seek_out",		FD_RSEEK_OUT,		FD_RAW_INTR },
#endif
#ifdef FD_RSEEK_IN
	{ "seek_in",		FD_RSEEK_IN,		FD_RAW_INTR },
#endif
#ifdef FD_LOCK
	{ "lock",		FD_LOCK,		0 },
	{ "unlock",		FD_UNLOCK,		0 },
#endif
	{ "specify",		FD_SPECIFY,		0 },
	{ "format",		FD_FORMAT,		FD_RAW_WRITE | FD_RAW_INTR },
	{ "version",		FD_VERSION,		0 },
	{ "configure",	FD_CONFIGURE,		0 },
	{ "perpendicular",	FD_PERPENDICULAR,	0 },
	{ "readid",	        FD_READID,     		FD_RAW_INTR },
	{ "dumpregs",	        FD_DUMPREGS,   		0 },
#ifdef FD_SAVE
	{ "save",	        FD_SAVE,   		0 },
	{ "partid",	        FD_SAVE,   		0 },
#endif
#ifdef FD_FORMAT_N_WRITE
	{ "format_n_write",  	FD_FORMAT_N_WRITE,     	FD_RAW_WRITE | FD_RAW_INTR },
	{ "restore",		FD_RESTORE,		0 },
	{ "powerdown",	FD_POWERDOWN,		0 },
	{ "option",		FD_OPTION,		0 },
	{ "drivespec",	FD_DRIVESPEC,		0 },
#endif
	{ 0,			0,			0 }
};

int print_time = 0;
int do_buffer = 0;
int do_short = 0;
int tostdout = 0;

FILE *f;

int lookup(char *name, int *cmd, int *flags)
{
	struct lookup_table *ptr;  

	ptr = tabl;

	while ( ptr->name && strcmp(ptr->name, name) )
		ptr++;

	if ( ! ptr->name)
		return 0;

	*cmd = ptr->cmd;
	*flags = ptr->flags;
	return 1;
}

long long new_gettime(void)
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
  
void print_result(struct floppy_raw_cmd *raw_cmd, long long date)
{
	int i;

	if ( do_short ){
		if ( raw_cmd->flags & ( FD_RAW_READ | FD_RAW_WRITE ))
			fprintf(f,"%lu ", raw_cmd->length );  
		for( i=0; i< raw_cmd->reply_count; i++ )
			fprintf(f,"0x%02x ", raw_cmd->reply[i] );
#ifdef FD_RAW_DISK_CHANGE
		if( raw_cmd->flags & FD_RAW_DISK_CHANGE)
			fprintf(f,"disk_change ");
		else
			fprintf(f,"no_disk_change ");
#endif
	} else {    
		if ( raw_cmd->flags & ( FD_RAW_READ | FD_RAW_WRITE ))
			fprintf(f,"remaining= %lu\n", raw_cmd->length );  
		for( i=0; i< raw_cmd->reply_count; i++ )
			fprintf(f,"%d: %x\n", i, raw_cmd->reply[i] );
#ifdef FD_RAW_DISK_CHANGE
		if( raw_cmd->flags & FD_RAW_DISK_CHANGE)
			fprintf(f,"disk change\n");
		else
			fprintf(f,"no disk change\n");
#endif
	}
	if ( print_time )
		fprintf(f,"%Ld\n", date);
	else
		fprintf(f,"\n");
}

int main(int argc, char **argv)
{
	int have_flags=0;
	int delay=0;
	struct timeval tv;
	long long start_date;
	int i,tmp, flags, cmd, size, fd;
	char *drive,*s,*end;
	int repeat=1;
	struct floppy_raw_cmd raw_cmd;
	int r_flags;
	int e_flags;
	struct floppy_raw_cmd *results=0;
	long long *times=0;
	long long *timesb4=0;
	int fm_mode = 0;
	int nomt_mode = 0;
	char *eptr;
	char buffer[ 512 * 2 * 24 ];


	f = stderr;

	raw_cmd.length= 512 * 2 * 24;
	raw_cmd.data = buffer;
	r_flags = e_flags = 0;
	raw_cmd.rate = 0;
	raw_cmd.track = 0;
	raw_cmd.cmd_count = 0;

	drive="/dev/fd0";

	if (( s=getenv("length") ) || (s = getenv("LENGTH")) ) {
		raw_cmd.length= strtoul( s,& eptr, 0);
		switch(*eptr) {
			case 'b':
				raw_cmd.length  *= 512;
				break;
			case 'k':
			case 'K':
				raw_cmd.length  *= 1024;
				break;
		}
	}

  
	if ((s = getenv("rate") ) || (s = getenv("RATE")) )
		raw_cmd.rate = strtoul( s,0,0);

	if ((s = getenv("track")) || (s = getenv("TRACK")) ||
		(s = getenv("cylinder")) || (s = getenv("CYLINDER"))) {
		e_flags |= FD_RAW_NEED_SEEK;
		raw_cmd.track = strtoul( s,0,0);
	}

	if ((s = getenv("drive") ) || (s = getenv("DRIVE")) )
		drive = s;

	fd = -1;
	while(argc){
		argv++;
		argc--;
		r_flags = 0;
		have_flags = 0;
		for ( ; argc; argc--, argv++){
			if (strcmp(";" , *argv ) == 0 ){
				break;
			}

			if ((lookup( *argv, &cmd, &flags ))){
					
				/* if this is a command, and we don't have a command
				 * yet, use it as such */
				if (cmd && raw_cmd.cmd_count == 0 ){
					raw_cmd.cmd[raw_cmd.cmd_count++] = cmd;
					if(!have_flags)
						/* use the default flags from the command, if
						 * we don't have an explicit flag set yet */
						r_flags |= flags;
				} else {
					/* this is not a command.  Always or it to the set of
					 * of flags */
					have_flags = 1;
					r_flags |= flags;
				}
				continue;
			}

			if ( strncmp( "length=", *argv, 7 ) == 0 ){
				raw_cmd.length= strtoul( (*argv)+7,&eptr, 0);
				switch(*eptr) {
					case 'b':
						raw_cmd.length  *= 512;
						break;
					case 'k':
					case 'K':
						raw_cmd.length  *= 1024;
						break;
				}
				continue;
			}
      
			if ( strncmp( "rate=", *argv, 5 ) == 0 ){
				raw_cmd.rate= strtoul( (*argv)+5,0,0);
				continue;
			}
      
			if ( strncmp( "track=", *argv, 6 ) == 0 ){
				r_flags |= FD_RAW_NEED_SEEK;
				raw_cmd.track= strtoul( (*argv)+6,0,0);
				continue;
			}

			if ( strncmp( "cylinder=", *argv, 9 ) == 0 ){
				r_flags |= FD_RAW_NEED_SEEK;
				raw_cmd.track= strtoul( (*argv)+9,0,0);
				continue;
			}

			if ( strncmp( "repeat=", *argv, 7 ) == 0 ){
				repeat= strtoul( (*argv)+7,0,0);
				continue;
			}

			if ( strncmp( "delay=", *argv, 6 ) == 0 ){
				delay= strtoul( (*argv)+6,0,0);
				continue;
			}
      
			if ( strncmp( "drive=", *argv, 6 ) == 0){
				drive=(*argv)+6;
				continue;
			}

			if ( strcmp( "print_time", *argv) == 0){
				print_time = 1;
				continue;
			}

			if ( strcmp( "do_buffer", *argv) == 0){
				do_buffer = 1;
				continue;
			}

			if ( strcmp( "fm", *argv) == 0){
				fm_mode = 1;
				continue;
			}


			if ( strcmp( "no-mt", *argv) == 0){
				nomt_mode = 1;
				continue;
			}



			if ( strcmp( "short", *argv) == 0){
				do_short = 1;
				continue;
			}
      
			if ( strcmp( "to_stdout", *argv) == 0){
				f = stdout;
				continue;
			}
      
			if ( *argv[0] == '/' ){
				drive = *argv;
				continue;
			}
      
			cmd = strtoul( *argv, &end, 0);
			if ( *end ){
				fprintf(stderr,"Unrecognized keyword: %s\n", *argv );
				exit(1);
			}
    
			if ( end == *argv )
				continue;
      
			raw_cmd.cmd[raw_cmd.cmd_count++] = cmd;
		}

		if(fm_mode)
			raw_cmd.cmd[0] &= ~0x40;
		if(nomt_mode)
			raw_cmd.cmd[0] &= ~0x80;
    
		if ( r_flags & FD_RAW_WRITE ){
			size = 0;
			while(1){
				tmp=read(0, buffer+size, raw_cmd.length - size );
				if ( tmp == 0 ){
					raw_cmd.length = size ;
					break;
				}
	
				if ( tmp < 0 ){
					perror("read");
					exit(1);
				}
				size += tmp;
				if ( size == raw_cmd.length )
					break;
			}
		} else
			size = raw_cmd.length;
    
		if ( *drive ){
			if ( fd >= 0 )
				close(fd);
			fd = strtoul(drive, &end, 0);
      
			if ( *end ){
				fd = open( drive, O_ACCMODE | O_NDELAY);
				if ( fd < 0 ){
					perror("open floppy");
					exit(1);
				}
			}
		}

		if (do_buffer){
			timesb4 =(long long *) calloc(repeat, sizeof(long long));
			times =(long long *) calloc(repeat, sizeof(long long));
			results = (struct floppy_raw_cmd *) 
				calloc(repeat, sizeof(struct floppy_raw_cmd));
			if ( !times || !results){
				fprintf(stderr,"Out of memory error\n");
				exit(1);
			}
		}
    
		start_date = new_gettime();
		for ( i=0; i< repeat; i++){
			if(do_buffer)
				timesb4[i] = new_gettime() - start_date;
			raw_cmd.flags = r_flags;
			tmp = ioctl( fd, FDRAWCMD, & raw_cmd );
			if ( tmp < 0 ){
				perror("raw cmd");
				exit(1);
			}
      
			if ( !do_buffer ){
				if ( r_flags & ( FD_RAW_READ ))
					write(1, buffer, size );
				print_result(&raw_cmd, new_gettime() - start_date);
			} else {
				times[i] = new_gettime() - start_date;
				results[i] = raw_cmd;
			}
			if (delay){
				tv.tv_sec = 0;
				tv.tv_usec = delay;
				select(0, 0,0,0, &tv);
			}
		}
    
		if (do_buffer) {
			for(i=0; i < repeat; i++){
				print_result(results + i, times[i]);
			}
		}
	}
	close(fd);

	exit(0);
}
