#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include "enh_options.h"
static struct option *my_longopts;
static char **my_xopts;
static char *my_shortopts;
static int length;

void print_usage(char *progname, struct enh_options *eo, char *userparams)
{
	int i;
	
	fprintf(stderr,"Usage: %s ", progname);
	for (i=0; i<length; i++){
		fprintf(stderr,"[-");
		if (eo[i].shorto )
			fprintf(stderr,"%c",eo[i].shorto);
		else
			fprintf(stderr,"-%s",eo[i].longo);
		if ( eo[i].has_arg ){
			if( eo[i].type & EO_TYPE_LIST )
				fprintf(stderr," list_of_");
			else
				fprintf(stderr," ");
			switch(eo[i].type){
			case EO_TYPE_CHAR:
				fprintf(stderr,"char");
				break;
			case EO_TYPE_STRING:
				fprintf(stderr,"string");
				break;
			case EO_TYPE_FLOAT:
				fprintf(stderr,"float");
				break;
			case EO_TYPE_DOUBLE:
				fprintf(stderr,"double");
				break;
			case EO_TYPE_BYTE:
				fprintf(stderr,"byte");
				break;
			case EO_TYPE_SHORT:
				fprintf(stderr,"short");
				break;
			case EO_TYPE_LONG:
				fprintf(stderr,"long");
				break;
			case EO_TYPE_FILE:
			case EO_TYPE_FILE_OR_FD:
				fprintf(stderr,"file");
				break;
			}
		}
		fprintf(stderr,"] ");
	}
	fprintf(stderr,"%s\n", userparams);
	fprintf(stderr,"Type %s --help to get a more complete help\n",
		progname);
}

static void parse_option_table(struct enh_options *eo)
{
	int i;
	char *ptr;

	/* first count them */
	for( i=0; eo[i].longo; i++);
	length = i;
	
	my_longopts = (struct option *)calloc(length+2, sizeof(struct option));
	if ( my_longopts == 0 ){
		fprintf(stderr,"out of memory\n");
		exit(1);
	}
	for(i=0; i<length; i++){
		my_longopts[i].val = eo[i].shorto;
		my_longopts[i].name = eo[i].longo;
		my_longopts[i].has_arg = eo[i].has_arg;
		my_longopts[i].flag = 0;		
	}
	my_longopts[length].val = 'h';
	my_longopts[length].name = "help";
	my_longopts[length].has_arg = 0;
	my_longopts[length].flag = 0;
	my_longopts[length+1].name = 0;

	my_xopts = (char **) calloc(length, sizeof(char *));
	if ( !my_xopts){
		fprintf(stderr,"out of memory\n");
		exit(1);
	}
	for (i=0; i<length; i++)
 		my_xopts[i]= 0;

	ptr = my_shortopts = (char *)malloc( 2 * length + 2 );
	if ( !ptr ){
		fprintf(stderr,"out of memory\n");
		exit(1);
	}
	for ( i=0; i<length; i++){
		if ( !eo[i].shorto )
			continue;		
		*ptr++ = eo[i].shorto;
		if ( eo[i].has_arg )
			*ptr++ = ':' ;
	}
	*ptr++='h';
	*ptr='\0';
}


static int parse_option(struct enh_options *eo, 
			 char *name)
{
	char *end;
	char *nextname;
	int number;
	int i, iterations;
	int flags;
	double d;
	float f;
	
	nextname = name - 1;

	if ( eo->type & EO_TYPE_LIST )
		iterations = eo->arg;
	else
		iterations = 1;

	for( i=0; i < iterations; i++ ){
		if ( !nextname )
			break;
		name = nextname+1;
		if ( eo->type & EO_TYPE_LIST )
			nextname = strchr(name, ',' );
		else
			nextname = 0;

		switch(eo->type & EO_DTYPE){
		case EO_TYPE_NONE:
			return 0;
		case EO_TYPE_STRING:
			if ( nextname )
				*nextname='\0';
			*((char **) eo->address) = name;
			if ( nextname)
				*nextname=',';
			continue;			
		}

		if (eo->has_arg){
			if(!*name ) /* last field is empty */
				break;
			if ( name == nextname ) /* field to be skipped */
				continue;
		}

		switch(eo->type & EO_DTYPE){
		case EO_TYPE_FLOAT:
			if( sscanf(name, "%f", &f ) != 1 ){
				fprintf(stderr,"%s is not a float\n", name);
				return -1;
			}
			((float *) eo->address)[i] = f;
			continue;
		case EO_TYPE_DOUBLE:
			if ( sscanf(name, "%lf", &d) != 1 ){
				fprintf(stderr,"%s is not a double\n", name);
				return -1;
			}
			((double *) eo->address)[i]=d;
			continue;
		case EO_TYPE_CHAR:
			*((char *) eo->address ) = name[0];
			continue;
		case EO_TYPE_BITMASK_BYTE:
			if ( eo->arg >= 0 )
				((char *) eo->address)[i] |= eo->arg & 0xff;
			else
				((char *) eo->address)[i] &= eo->arg;
			return 0;
		case EO_TYPE_BITMASK_SHORT:
			if ( eo->arg >= 0 )
				((short *) eo->address)[i] |= eo->arg & 0xff;
			else
				((short *) eo->address)[i] &= eo->arg;
			return 0;
		case EO_TYPE_BITMASK_LONG:
		if ( eo->arg >= 0 )
			((long *) eo->address)[i] |= eo->arg & 0x7fff;
		else
			((long *) eo->address)[i] &= eo->arg;
			return 0;
		}

		if ( ! eo->has_arg ) 
			number = eo->arg;
		else
			number = strtoul(name, &end, 0 );
		
		switch( eo->type & EO_DTYPE){
		case EO_TYPE_FILE_OR_FD:
			if ((!*end || end == nextname) && number >= 0){
				((int *) eo->address)[i] = number;
				continue;
			}
			/* fall through */
		case EO_TYPE_FILE:
			if (nextname)
				*nextname='\0';
			if ( eo->type & EO_TYPE_LIST )
				flags = O_RDONLY;
			else
				flags = eo->arg;
			number = open(name, flags, 0644 );
			if (nextname)
				*nextname=',';
			if ( number < 0 ){
				perror(name);
			}
			((int *) eo->address)[i] = number;
			continue;
		}

		if (eo->has_arg && end != nextname && *end){
			fprintf(stderr,"%s is not a number\n", name);
			return -1;
		}

		switch(eo->type & EO_DTYPE){
		case EO_TYPE_BYTE:
			((char *) eo->address)[i] = number;
			continue;
		case EO_TYPE_SHORT:
			((short *) eo->address)[i] = number;
			continue;
		case EO_TYPE_LONG:
			((long *) eo->address)[i] = number;
			continue;
		default:
			fprintf(stderr,
				"Bad option type in parse_option for %s\n",
				eo->longo);
			exit(1);
		}
	}
	if ( eo->type & EO_TYPE_LIST )
		eo->arg = i;
	return 0;
}

static int get_wordlength(struct enh_options *eo)
{
	int wordlength=0;
	int i, wl;

	for( i=0; i<length; i++){
	  wl =  strlen(eo[i].longo);
	  if( wl > wordlength )
	    wordlength = wl;
	}
	return wordlength;
}

static void print_option(struct enh_options *eo, int wordlength)
{
	int i, iterations;
	long number;
	
	if ( eo->type & EO_TYPE_LIST )
		iterations = eo->arg;
	else
		iterations = 1;

	if ( eo->has_arg ){
		printf("%s=", eo->longo);
		for(i=strlen(eo->longo); i<wordlength;  i++)
			putchar(' ');
	}

	for(i=0; i < iterations; i++){
		if ( i )
			printf(",");
		switch( eo->type & EO_DTYPE ){
		case EO_TYPE_BITMASK_BYTE:
			if ( eo->arg >= 0 &&
			    ((char *) eo->address)[i] & eo->arg)
				printf("%s\n", eo->longo);
			return;
		case EO_TYPE_BITMASK_SHORT:
			if ( eo->arg >= 0 &&
			    ((short *) eo->address)[i] & eo->arg)
				printf("%s\n", eo->longo);
			return;
		case EO_TYPE_BITMASK_LONG:
			if ( eo->arg >= 0 &&
			    ((long *) eo->address)[i] & eo->arg)
				printf("%s\n", eo->longo);
			return;
		}

		switch( eo->type & EO_DTYPE ){
		case EO_TYPE_FLOAT:
			printf("%f", ((float *) eo->address)[i] );
			continue;
		case EO_TYPE_DOUBLE:
			printf("%f", ((double *) eo->address)[i] );
			continue;
		case EO_TYPE_STRING:
			printf("%s", ((char **) eo->address)[i] );
			continue;
		case EO_TYPE_FILE:
		case EO_TYPE_FILE_OR_FD:
			number = ((int *) eo->address)[i];
			if ( number < 0 )
				printf("<closed>");
			else
				printf("%%%ld", number);
			continue;
		}

		switch( eo->type & EO_DTYPE ){
		case EO_TYPE_BYTE:
			number = ((char *) eo->address)[i] ;
			break;
		case EO_TYPE_SHORT:
			number = ((short *) eo->address)[i] ;
			break;
		case EO_TYPE_LONG:
			number = ((long *) eo->address)[i] ;
			break;
		case EO_TYPE_CHAR:
			number = ((char *) eo->address)[i] ;
			break;
		default:
			printf("??\n");
			return;
		}
		if ( eo->has_arg ){
			if ( (eo->type & EO_DTYPE ) ==
			    EO_TYPE_CHAR )
				printf("%c", (char) number);
			else
				printf("%ld", number);
		} else {
			if ( eo->arg == number )
				printf("%s", eo->longo );
			else return;
		}
	}
	printf("\n");
}


int parse_delayed_options(struct enh_options *eo, int mask)
{
	int i;
	int ret=0;
	for(i=0; i<length; i++)
		if ((eo[i].mask & mask) &&
		    (eo[i].type & EO_TYPE_DELAYED ) &&
		    my_xopts[i] )
			ret |= parse_option(eo+i, my_xopts[i]);
	return ret;
}

void print_current_settings(struct enh_options *eo, int mask)
{
	int i,wl;
	wl = get_wordlength(eo);
	for(i=0; i<length; i++)
		if (eo[i].mask & mask)
			print_option(eo+i,wl);
}

void print_help(struct enh_options *eo)
{
	int i, wordlength, htlength, col;
	char *ptr1, *ptr2;
	char format[20];

	wordlength = get_wordlength(eo);
	snprintf(format, 19, "--%%-%ds ", wordlength);
	htlength=80 - wordlength - 6;
	for(i=0; i<length; i++){
		if ( eo[i].shorto )
			printf("-%c ", eo[i].shorto );
		else
			printf("   ");
		printf(format, eo[i].longo);
		ptr1=eo[i].helptext;
		if (!ptr1){
			putchar('\n');
			continue;
		}
		col=wordlength+6;
		while(1){
			/* first print the first word */
			while (*ptr1 && *ptr1 != ' ' && col < 80){
				col++;
				putchar(*ptr1++);
			}
			ptr2=ptr1;
			while (1){
				if ( !*ptr2 || *ptr2 == ' ' )
					while( ptr1 < ptr2 )
						putchar(*ptr1++);
				if ( col == 80 || !*ptr2 ){
					putchar('\n');
					ptr2 = ptr1;
					break;
				}
				if ( *ptr1 == ' ' )
					putchar(*ptr1++);
				col++;
				ptr2++;
			}
			if ( !*ptr1 )
				break;
				
			if (col == 80 ){
				for(col=0; col<wordlength+6; col++)
					putchar(' ');
				while(*ptr1 == ' ' )
					ptr1++;
				ptr2=ptr1+80;
			}
		}
	}
}

static int find_offset(struct enh_options *eo, char *name)
{
	int i;
	for(i=0; i<length; i++){
		if ( strcmp( eo[i].longo, name ) == 0 )
			return i;
	}
	fprintf(stderr,"non existent option %s\n", name);
	return -1;
}

int set_length(struct enh_options *eo, char *name, int length)
{
	int i;

	i=find_offset(eo, name);
	if ( i<0)
		return -1;
	eo[i].arg = length;
	return 1;
}

int get_length(struct enh_options *eo, char *name)
{
	int i;

	i=find_offset(eo, name);
	if ( i<0)
		return -1;
	if ( ! my_xopts[i] )
		return -1;
	return eo[i].arg;
}

int getopt_enh(int argc, 
	       char **argv, 
	       struct enh_options *eo, 
	       int *longind,
	       int *mask,
	       char *userparams)
{
	int ch;
	char *s;
	int option_index;
	int ret=0;

	if ( !my_shortopts )
		parse_option_table(eo);

	while (1){
		option_index = -1;
		ch = getopt_long (argc, argv, my_shortopts, my_longopts, 
				  & option_index);
		if ( ch == EOF )
			break;

		switch(ch){
		case 'h':
			print_help(eo);
			exit(0);
		case '?':
			ret = 1;
			continue;
		}
		if ( option_index == -1 )
			/* we have probably a short option here */
			for (option_index=0;
			     option_index < length && 
			     eo[option_index].shorto != (char) ch;
			     option_index++);

		if ( my_xopts[option_index] ){
			fprintf(stderr,"option %s given twice\n",
				eo[option_index].longo);
			ret = 1;
			continue;
		}

		if ( optarg )
			my_xopts[option_index] = optarg;
		else
			my_xopts[option_index] = argv[0];
		*mask |= eo[option_index].mask;

		if ( eo[option_index].type & EO_TYPE_DELAYED )
			continue;
		if ( eo[option_index].type == EO_TYPE_MANUAL ){
			if ( longind )
				*longind = option_index;
			return ch;
		}
		ret |= parse_option(eo+option_index, optarg);
	}
	

	/* setting of variables out of environment */
/* Commented out on 1998-08-28 by AF, Re: Debian Bug#12166
	for( option_index=0; option_index<length; option_index++){
		if ( !my_xopts[option_index] ){
			s=getenv(eo[option_index].longo);
			if ( s ){
				my_xopts[option_index] = optarg;
				if ( eo[option_index].type & EO_TYPE_DELAYED )
					continue;
				parse_option(eo+option_index, s);
			}
		}
	}
*/
	if ( ret ){
		print_usage(argv[0], eo, userparams);
		return '?';
	} else
		return EOF;
}
