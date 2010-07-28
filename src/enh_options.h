#define EO_TYPE_NONE 0 /* no option argument. Only the mask parameter is set */

#define EO_TYPE_BYTE 1
#define EO_TYPE_SHORT 2
#define EO_TYPE_LONG 3

#define EO_TYPE_FLOAT 4
#define EO_TYPE_DOUBLE 5

/* bit mask arguments. The data is or'ed with the mask stored in arg if
 * arg is positive, and anded with its complement otherwise */
#define EO_TYPE_BITMASK_BYTE 6
#define EO_TYPE_BITMASK_SHORT 7
#define EO_TYPE_BITMASK_LONG 8

#define EO_TYPE_CHAR 9
#define EO_TYPE_STRING 10

/* file arguments. Optarg is interpreted as a filename which is open using
 * the flags described in arg */
#define EO_TYPE_FILE 11

/* same as FILE except that the user may also give an integer, which will
 * be interpreted as an already open file descriptor */
#define EO_TYPE_FILE_OR_FD 12

#define EO_TYPE_MANUAL 13 /* client wants to treat this option himself */

/* list arguments. Optarg is interpreted as a comma separated list of
 * elementary types. */
#define EO_TYPE_LIST 0x40
#define EO_TYPE_DELAYED 0x80
#define EO_DTYPE (~(EO_TYPE_LIST | EO_TYPE_DELAYED ))

struct enh_options {
	char shorto; /* short option may be zero */
	char *longo; /* long option. set this to zero for last option */
	int has_arg; /* does the option have an argument? */
	
	int type; /* data type of optarg */
	int arg;  /* complement to type (length of list, etc) */
	int mask; /*to select certain entries */
	void *address;
	char *helptext;
};


int parse_delayed_options(struct enh_options *eo, int mask);
void print_current_settings(struct enh_options *eo, int mask);
/* mask is used to selectively only process those options whose mask have
 * at least one bit in common. Set mask to -1 if you want to process every
 * option */

void  print_help(struct enh_options *eo);

void print_usage(char *progname, struct enh_options *eo, char *userparams);
/* userparams represent the non-option parameters of the command. Printed
 * after the automatically generated list */

int set_length(struct enh_options *eo, char *name, int length);
/* sets the length of the list to be printed by print_options */

int get_length(struct enh_options *eo, char *name);
/* reads the length of the list entered by the user */

int getopt_enh(int argc, char **argv, struct enh_options *eo, 
	       int *longind, int *mask, char *userparams);

/* 
 * longind returns the long index of the current option
 * mask returns the mask resulting from all the encountered options
 * userparams are used for print_usage */

