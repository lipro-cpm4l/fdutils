#include "../config.h"
#include <asm/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <malloc.h>
#include <stdarg.h>
#include <mntent.h>
#include <getopt.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <linux/fd.h>
#include <linux/minix_fs.h>


#ifdef HAVE_LINUX_EXT_FS_H
#include <linux/ext_fs.h>
#endif

#include <linux/ext2_fs.h>

#ifdef HAVE_LINUX_XIA_FS_H
#include <linux/xia_fs.h>
#endif

#include <syslog.h>
#include <sys/types.h>
#include <grp.h>


#define USE_2M
#include "msdos_fs.h"

/* if set to 1, fdmount will chown the mount point to the user who
   did the mount, and set permissions according to his umask.
*/
#define SETOWNER 1

/* if set to 1, the mount point for fdmount must be owned by the user 
   unless it is the default. Leave this on if you are concerned about
   security.
*/
#define CHKOWNER_MOUNT 1

/* if set to 1, the mount point for fdumount must be owned by the user
   _even if_ it is the default. Should be used only with SETOWNER.
*/
#define CHKOWNER_UMOUNT 1

/* by default, drive names are fd0..7, device names /dev/fd0../dev/fd7,
   and default mountpoints /fd0../fd7
*/
#define NAME_PATTERN               "fd%d"
#define DEFAULT_DIR_PATTERN        "/fd%d"
#define DEVICE_PATTERN             "/dev/fd%d"

#define DEBUG 0

/***********************************************************************/


#define UNKNOWN -1

#define INRANGE(x,lo,hi) ((unsigned)((x)-(lo))<=(hi))

typedef unsigned short word;
typedef unsigned char byte;

typedef struct floppy_struct format;

typedef struct mnt_node {
    struct mnt_node *next;
    struct mntent ms;
} mnt_node;

typedef struct {
    int sect;
    int tracks;
    int heads;
    int totsect;
    int _2m;
    int sectsizec;
} fmt_descr;
   
int locked=0;
char curdev[100] = "";
char *progname = NULL;
int opt_silent=0;
int use_syslog = 0;
int opt_detach = 0;
int ruid=-1;
int rgid=-1;
int _umask=077;

enum { T_NULL, T_MINIX, T_DOS, T_VFAT, T_EXT2, T_EXT, T_XIA };
char *fsnames[7] = {"???","minix", "msdos", "vfat", "ext2","ext","xia"};

void die(const char *text,...) {
    char buff[140];
    va_list p;
    va_start(p,text);
    vsnprintf(buff,139,text,p);
    va_end(p);
    if(use_syslog)
	syslog(LOG_ERR, "%s: %s\n",curdev,buff);
    else
	fprintf(stderr,"%s (%s): %s\n",progname,curdev,buff);
    exit(1);
}

void msg(char *text,...) {
    char buff[140];
    va_list p;
    va_start(p,text);
    vsnprintf(buff,139,text,p);
    va_end(p);
    if(!opt_silent) {
	if(use_syslog)
	    syslog(LOG_INFO, "%s: %s\n",curdev,buff);
	else
	    fprintf(stderr,"%s (%s): %s\n",progname,curdev,buff);
    }   
}

void errmsg(char *text,...) {
    char buff[140];
    va_list p;
    va_start(p,text);
    vsnprintf(buff,139,text,p);
    va_end(p);
    if(use_syslog)
	syslog(LOG_ERR, "%s: %s\n",curdev,buff);
    else
	fprintf(stderr,"%s (%s): %s\n",progname,curdev,buff);
}


#if 0
void print_format(format *F) {
    static int rates[4] = { 500,300,250,1000 };
    printf("%d sectors\n"
	   "%d sectors per track\n"
	   "%d heads\n"
	   "%d tracks\n"
	   "double stepping: %s\n"
	   "gap1: %d, gap2: %d, rate: %d Kbps, cod: %s, perp: %d\n"
	   "sectsize: %d, 2M: %s, steprate: %d, hut: %d\n"
	   "format name: %s\n",
	   F->size, F->sect, F->head, F->track,
	   (F->stretch==UNKNOWN)?"?":(F->stretch?"yes":"no"),
	   F->gap, F->fmt_gap, rates[F->rate&0x03], (F->rate&0x80)?"FM":"MFM",
	   F->rate & FD_PERP, 128<<((((F->rate&0x38)>>3)+2)%8), 
	   (F->rate&FD_2M)?"yes":"no",F->spec1>>4, F->spec1%0x0F,
	   F->name?"<none>":F->name);
}
#else
#define print_format(f)
#endif

void *xmalloc(int size) {
    void *p = malloc(size);
    if (!p) die("out of memory");
    return p;
}

/* Make a canonical pathname from PATH.  Returns a freshly malloced string.
   It is up the *caller* to ensure that the PATH is sensible.  i.e.
   canonicalize ("/dev/fd0/.") returns "/dev/fd0" even though ``/dev/fd0/.''
   is not a legal pathname for ``/dev/fd0.''  */
/* shamelessly copied from mount */
char *
canonicalize (const char *path)
{
    char *canonical = xmalloc (PATH_MAX + 1);
    char *p = canonical;
  
    if (path == NULL)
	return NULL;

#if 0
    if (!strcmp (path, "none"))
	{
	    strncpy (canonical, path, sizeof(canonical)-1);
	    canonical[sizeof(canonical)-1] = '\0';
	    return canonical;
	}
    if (strchr (path, ':') != NULL)
	{
	    strncpy(canonical, path, sizeof(canonical)-1);
	    canonical[sizeof(canonical)-1] = '\0';
	    return canonical;
	}
#endif

    if (*path == '/')
	{
	    /* We've already got an absolute path in PATH, but we need at
	       least one char in canonical[] on entry to the loop below.  */
	    *p = *path++;
	}
    else
	{
	    getcwd (canonical, PATH_MAX);
	    p = canonical + strlen (canonical) - 1;
	    if (*p != '/')
		*++p = '/';
	}
  
    /* There is at least one character in canonical[],
       and the last char in canonical[], *p, is '/'.  */
    while ((*path != '\0') && (p < canonical + PATH_MAX))
	if (*p != '/')
	    {
		*++p = *path++;
	    }
	else
	    {
		if (path[0] == '/')
		    {
			path++;		/* eliminate duplicate slashes (``//'') */
		    }
		else if ((path[0] == '.') && ((path[1] == '\0') || (path[1] == '/')))
		    {
			path++;		/* eliminate ``/.'' */
		    }
		else if ((path[0] == '.') && (path[1] == '.')
			 && ((path[2] == '\0') || (path[2] == '/')))
		    {
			while ((p > canonical) && (*--p != '/'))
			    /* ascend on ``/../'' */
			    ;
			path += 2;
		    }
		else
		    {
			*++p = *path++;	/* not a special case, just copy char */
		    }
	    }
    if (p >= (canonical + PATH_MAX))
	die ("path too long");

    if (*p == '/')
	--p;			/* eliminate trailing slash */

    *++p = '\0';
  
    return canonical;
}

/**********************************************************************/

mnt_node *mounted_list = NULL, *mounted_list_end = NULL;
int mtab_changed;

const char *mtab_filename = "/etc/mtab";
const char *lock_filename = "/etc/mtab~";

mnt_node *get_mounted(const char *devname) {
    mnt_node *mnt;
    for(mnt=mounted_list;mnt;mnt=mnt->next)
	if (strcmp(mnt->ms.mnt_fsname,devname)==0) return mnt;
    return NULL;
}

void _dup(char **s) {
    *s = *s?strdup(*s):NULL;
}

void _free(char *s) {
    if (s) free(s);
}

void free_mtab_node(mnt_node *ent) {
    _free(ent->ms.mnt_fsname);
    _free(ent->ms.mnt_dir);
    _free(ent->ms.mnt_type);
    _free(ent->ms.mnt_opts);
    free(ent);
}

void add_mtab(const struct mntent *ent) {
    mnt_node *mm;
    mm=xmalloc(sizeof(mnt_node));
    if (!mounted_list)
	mounted_list=mounted_list_end=mm;
    else
	mounted_list_end->next=mm;
    mounted_list_end=mm;
    memcpy(&mm->ms,ent,sizeof(struct mntent));
    _dup(&mm->ms.mnt_fsname);
    _dup(&mm->ms.mnt_dir);
    _dup(&mm->ms.mnt_type);
    _dup(&mm->ms.mnt_opts);
    mm->next=NULL;
    mtab_changed=1;
}

/* remove all mtab entries with special entry 'devname'. */

void remove_mtab(const char *devname) {
    mnt_node *mm,*prev,*nxt;
    mm=mounted_list;
    prev=NULL;
    while(mm) {
	nxt=mm->next;
	if (strcmp(mm->ms.mnt_fsname,devname)==0) {
	    if (!prev)
		mounted_list=mm->next;
	    else
		prev->next=mm->next;
	    free_mtab_node(mm);
	}
	else {
	    prev=mm;
	}
	mm=nxt;
    }
    mounted_list_end=prev;
    mtab_changed=1;
}

/* read table of mounted drives from /etc/mtab and put it in mounted_list.
*/
void read_mtab() {
    FILE *mtab;
    struct mntent *ent;
    mounted_list=mounted_list_end=NULL;
    mtab=setmntent(mtab_filename,"r");
    while((ent=getmntent(mtab)))
	add_mtab(ent);
    endmntent(mtab);
    mtab_changed=0;
}

void save_mtab() {
    FILE *mtab;
    mnt_node *mnt,*next;
    mtab=setmntent(mtab_filename,"w");
    for(mnt=mounted_list;mnt;mnt=next) {
	addmntent(mtab,&mnt->ms);
	next=mnt->next;
	free_mtab_node(mnt);
    }
    mounted_list=mounted_list_end=NULL;
    endmntent(mtab);
    mtab_changed=0;
}

inline void lock_mtab() {
    int fd;
    if((fd = open(lock_filename, O_WRONLY | O_CREAT | O_EXCL, 0744)) < 0) {
	die("Cannot create lock file %s: %s",lock_filename,strerror(errno));
    }
    close(fd);
    locked=1;
}

inline void unlock_mtab() {
    unlink(lock_filename);
    locked=0;
}

void lock_read_mtab() {
    lock_mtab();
    read_mtab();
    mtab_changed=0;
}

void save_unlock_mtab() {
    if (mtab_changed) save_mtab();
    unlock_mtab();
}

/*********************************************************************/

int probe_drive(char *devname) {
    char drive_name[17];
    int e,fd,type;
   
    drive_name[16]=0;
    fd=open(devname,O_RDONLY | O_NDELAY);
    if (fd<=0) return 0;

    e=ioctl(fd,FDGETDRVTYP,(void*)drive_name);
    if (e) {
	errmsg("ioctl(FDGETDRVTYP) failed on %s: %s",devname,strerror(errno));
	close(fd);
	return 0;
    }

    close(fd);

    if (strchr("EDHQdhq",drive_name[0])) {
	type=atoi(drive_name+1);
	if (type==360 || type==720 || type==1200 || 
	    type==1440 || type==2880)
	    {
		return type;
	    }
    }
    errmsg("unknown drive type '%s' for %s",drive_name,devname);
    return 0;
}



/* identify the type of file system from the given boot sector/
   super block and read out the available format parameters
   into *fmt.
   super = first 2K of the disk.

   return the type if identified filesystem (T_DOS,T_MINIX,...)
   or 0 for error.
*/

int id_fstype(byte *super, fmt_descr *fmt) {

#define minix ((struct minix_super_block*)(super+1024))
#define dos   ((struct msdos_boot_sector*)super)
#define ext2  ((struct ext2_super_block*)(super+1024))
#define ext   ((struct ext_super_block*)(super+1024))
#define xia   ((struct xiafs_super_block*)(super+1024))

    memset(fmt,0,sizeof(*fmt));

    /* we look for Unix-type filesystems first because mkfs doesn't
       overwrite the first 1K of the disk, so a DOS filesystem might
       be detected even though the data area is overwritten.
       */

    /* look for Minix filesystem */

    if (minix->s_magic==MINIX_SUPER_MAGIC ||
	minix->s_magic==MINIX_SUPER_MAGIC2)
	{
	    fmt->totsect=minix->s_nzones * (2<<minix->s_log_zone_size);
	    return T_MINIX;
	}

    /* look for ext2 filesystem */

    if (ext2->s_magic==EXT2_SUPER_MAGIC
#ifdef EXT2_PRE_02B_MAGIC
	|| ext2->s_magic==EXT2_PRE_02B_MAGIC
#endif
	)
	{
	    fmt->totsect=ext2->s_blocks_count * (2<<ext2->s_log_block_size);
	    return T_EXT2;
	}

#ifdef EXT_SUPER_MAGIC
    /* look for ext filesystem */

    if (ext->s_magic==EXT_SUPER_MAGIC)
	{
	    fmt->totsect=ext->s_nzones * (2<<ext->s_log_zone_size);
	    return T_EXT;
	}
#endif

#ifdef _XIAFS_SUPER_MAGIC
    /* look for xia filesystem */

    if (xia->s_magic==_XIAFS_SUPER_MAGIC)
	{
	    fmt->totsect=xia->s_nzones * xia->s_zone_size/512;
	    return T_XIA;
	}
#endif
    /* add more format types here ... */

    /* look for MS-DOG filesystem 
       this is more a looking for hints than checking a 
       well-defined identification (which doesn't exist), and 
       I don't expect it to be 100% reliable. 
       */
   
    if (dos->bootid==0xAA55 ||             /* check boot sector id */
	(*(byte*)(super+512)>=0xF0 &&      /* check media descriptor */
	 *(word*)(super+513)==0xFFFF) ||
        strncmp(dos->fat_type,"FAT",3)==0) /* check FAT id string */ 
	{
	    int sect,heads,tracks,totsect,s_cyl;

	    totsect=dos->sectors;
	    if (totsect==0) totsect=dos->total_sect;
	    sect=dos->secs_track;
	    heads=dos->heads;
	    s_cyl=sect*heads;

	    /* sanity checks */
	    if (!INRANGE(heads,1,2) || 
		!INRANGE(sect,3,60) ||
		!INRANGE(totsect,100,10000) ||
		totsect % s_cyl != 0)
		{
		    /* try media descriptor (very old DOS disks) */
		    switch(super[512]) {
			case 0xfe:             /* 160k */
			    tracks=40; 
			    sect=8; 
			    heads=1;
			    break;
			case 0xfc:             /* 180k */
			    tracks=40;
			    sect=9;
			    heads=1;
			    break;
			case 0xff:             /* 320k */
			    tracks=40;
			    sect=8;
			    heads=2;
			    break;
			case 0xfd:             /* 360k */
			    tracks=40;
			    sect=9;
			    heads=2;
			    break;
			case 0xf9:             /* 1.2M */
			    tracks=80;
			    sect=15;
			    heads=2;
			    break;
			default:
			    goto no_dos;
		    }
		    totsect=tracks*sect*heads;
		}
	    else {
		tracks=totsect/s_cyl;
	    }

	    fmt->sect    = sect;
	    fmt->tracks  = tracks;
	    fmt->heads   = heads;
	    fmt->totsect = totsect;
	    fmt->_2m=0;
	    fmt->sectsizec=2;  /* 512 bytes */
      
	    /* check for 2M format */
	    if (strncmp(dos->banner,"2M",2)==0) {
		int inftm=dos->InfTm;
		if (INRANGE(inftm,76,510)) {
		    fmt->sectsizec=super[inftm];
		    if (!INRANGE(fmt->sectsizec,0,7)) fmt->sectsizec=2;
		}
		fmt->_2m=1;
	    }
	    else {
		fmt->sectsizec=2;
	    }
	    return T_DOS;
	}

no_dos:

    return 0;  /* disk format not identified. */
   
}

int chk_mountpoint(char *dir,int is_default) {
    int must_own;
    struct stat st;

    if (stat(dir,&st)!=0) {
	errmsg("Can't access %s: %s\n",dir,strerror(errno));
	return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
	errmsg("%s is not a directory\n",dir);
	return 0;
    }

    if (!is_default && access(dir,W_OK)!=0) {
	errmsg("No write permission to %s\n",dir);
	return 0;
    }

#if CHKOWNER_MOUNT
    /* user specified mount points must be owned by the user
       unless the user is root
       */
    must_own=(ruid!=0) && !is_default;
    if (must_own && st.st_uid!=ruid) {
	errmsg("Not owner of %s\n",dir);
	return 0;
    }
#endif

    return 1;
}

/* open the given device (which ought to be a floppy disk), figure out
   the format, and mount it.
   All message output here should be done via msg() and errmsg().
*/

#define DO_IOCTL(fd,code,parm)						\
{									\
	if (ioctl(fd,code,parm)!=0) {					\
		errmsg("ioctl(code) failed: %s",strerror(errno));	\
		goto err_close;                                 	\
	}								\
}

#define ADD_OPT(format,parameter) \
snprintf(options+strlen(options), MAX_OPT - strlen(options)-1, \
		"," format, parameter)

#define MAX_OPT 1024
char dos_options[MAX_OPT];
char ext2_options[MAX_OPT];


int do_mount(char *devname,char *_mountpoint,
             int flags,int force,int is_default,int drivetype) 
{
    int fd,e,fstype;
    format F;
    char *fsname;
    fmt_descr fmt;
    struct mntent ms;
    struct floppy_drive_struct drivstat;
    char options[80+MAX_OPT]; 
    char super[2048];
    char *mountpoint;
   
    strncpy(curdev,devname, sizeof(curdev));
    curdev[sizeof(curdev)-1]='\0';

    if (access(devname,R_OK)!=0)
	die("no access to %s",devname);
   
    if (!(flags&MS_RDONLY) && access(devname,W_OK)!=0)
	die("no write access to %s",devname);
      
    lock_read_mtab();
   
    if(get_mounted(devname)) {
	if (!force) {
	    errmsg("already mounted");
	    goto err_unlock;
	}
	else {
	    msg("already in /etc/mtab, trying to mount anyway!");
	}
    }

    mountpoint=canonicalize(_mountpoint);
   
    if (!chk_mountpoint(mountpoint,is_default)) goto err_unlock;

    /* all right, it's ok to mount. Now try to figure out
       the details.
       */

    fd=open(devname,O_RDONLY);
    if (fd<0) {
	errmsg("error opening device: %s",strerror(errno));
	goto err_unlock;
    }

    errno=0;   
    lseek(fd,0,SEEK_SET);
    read(fd,super,sizeof(super));
    if (errno) {
	errmsg("error reading boot/super block: %s",strerror(errno));
	goto err_close;
    }

    /* check if disk is write protected
       note: we don't need to poll (FDPOLLDRVSTAT) here because 
       the previous super block read updated the state.
       */
    DO_IOCTL(fd,FDGETDRVSTAT, &drivstat);
    if (!(drivstat.flags & FD_DISK_WRITABLE)) flags|=MS_RDONLY;

    /* get the auto-detected floppy parameters */
    DO_IOCTL(fd,FDGETPRM,&F);

#if DEBUG
    printf("autodetected format:\n\n");
    print_format(&F);
#endif
   
    fstype=id_fstype(super,&fmt);
    if (fstype==0) {
	errmsg("unknown filesystem type");
	goto err_close;
    }
    fsname=fsnames[fstype];

    if (fstype==T_DOS) {
	F.sect  = fmt.sect;
	F.track = fmt.tracks;
	F.head  = fmt.heads;
	F.size  = fmt.totsect;
	F.rate &= ~FD_2M;
	if (fmt._2m) {
	    F.rate &= ~0x38;
	    F.rate |= (((fmt.sectsizec+6)%8)<<3) | FD_2M;
	}
    }
    else {
	/* hope that the track layout was detected correctly and figure
	   out the number of tracks from the fs size.
	   */
	int s_cyl = F.sect*F.head;
	int tr;

	if (!s_cyl) goto err_close;

	tr=fmt.totsect/s_cyl;
	if (fmt.totsect%s_cyl==0 && INRANGE(tr,30,83)) {
	    /* was detected OK! */
	    F.track=tr;
	    F.size=fmt.totsect;
	}
	else {
	    errmsg("sorry, can't figure out format (%s filesystem)", fsname);
	    goto err_close;
	}
    }
    F.stretch = (drivetype!=360 && F.track<43);

#if DEBUG
    printf("setting format:\n\n");
    print_format(&F);
#endif
   
    DO_IOCTL(fd,FDSETPRM,&F);

    close(fd);  


    /* prepare the /etc/mtab entry and mount the floppy.
     */

    if (fstype==T_DOS) flags &= ~(MS_NOEXEC|MS_NODEV);
   
    *options=0;
    strcat(options,(flags&MS_RDONLY) ? "ro"      : "rw");
    strcat(options,(flags&MS_NOSUID) ? ",nosuid" : "");
    strcat(options,(flags&MS_NODEV)  ? ",nodev"  : "");
    strcat(options,(flags&MS_NOEXEC) ? ",noexec" : "");
    strcat(options,(flags&MS_SYNCHRONOUS) ? ",sync" : "");

    if(fstype == T_EXT2) {
	ADD_OPT("resuid=%d", ruid);
	/* resuid doesn't change the owner of the fs, but rather names the
	 * user who is allowed to fill up the fs more than 100%.
	 * This is just fine for use as a marker */
	strcat(options, ext2_options);
    }
    if(fstype == T_DOS)
	strcat(options, dos_options);

    if(fstype != T_EXT2) {
	/* Unfortunately, ext2 barfs at options it doesn't understand */
	ADD_OPT("uid=%d", ruid);
	ADD_OPT("gid=%d", rgid);
	ADD_OPT("umask=%03o", _umask);
    }

    e=mount(devname,mountpoint,fsname,flags|MS_MGC_VAL,NULL);
    if (e && fstype==T_DOS) {
        fsname=fsnames[T_VFAT];
        e=mount(devname,mountpoint,fsname,flags|MS_MGC_VAL,NULL);
    }
    if (e) {
	errmsg("failed to mount %s %dK-disk: %s",
	       fsname,F.size/2,strerror(errno));
	goto err_unlock;
    }

#if SETOWNER
    if(fstype != T_DOS && !(flags&MS_RDONLY)) {
	e=chown(mountpoint,ruid,rgid);
	if (e) msg("warning: chown failed");
    }
    if(fstype != T_DOS && !(flags&MS_RDONLY)) {
	e=chmod(mountpoint,~_umask & 0777);
	if (e) msg("warning: chmod failed");
    }
#endif
   
    msg("mounted %s %dK-disk (%s) on %s",fsname,F.size/2,
	(flags&MS_RDONLY) ? "readonly":"read/write", mountpoint);

    /* add entry to /etc/mtab */
    ms.mnt_fsname=devname;
    ms.mnt_dir=mountpoint;
    ms.mnt_type=fsname;
    ms.mnt_opts=options;
    ms.mnt_freq=0;
    ms.mnt_passno=0;
    add_mtab(&ms);

    save_unlock_mtab();
    return 0;

err_close:
    close(fd);
err_unlock:
    save_unlock_mtab();
    return -1;
}

int do_umount(const char *devname,int force) {
    int e,fuid;
    mnt_node *mnt;
    char *mountpoint, *uidstr;
    struct stat st;

    lock_read_mtab();
    strncpy(curdev,devname, sizeof(curdev));
    curdev[sizeof(curdev)-1]='\0';


    mnt=get_mounted(devname);
    if (!mnt) {
	errmsg("not mounted");
	save_unlock_mtab();
	return -1;
    }
    else {
	mountpoint=mnt->ms.mnt_dir;

#if CHKOWNER_UMOUNT
	e=stat(mountpoint,&st);
	if (e) {
	    errmsg("Cannot access %s: %s\n",mountpoint,strerror(errno));
	    goto err;
	}
	uidstr = strstr(mnt->ms.mnt_opts, ",uid=");
	if(uidstr)
	    fuid = atoi(uidstr+5);
	else {
	    uidstr = strstr(mnt->ms.mnt_opts, ",resuid=");
	    if(uidstr)
		fuid = atoi(uidstr+8);
	    fuid = 0;
	}
	if (ruid && st.st_uid!=ruid && fuid != ruid) {
	    errmsg("Not owner of mounted directory: UID=%d\n",st.st_uid);
	    goto err;
	}
    }
#endif
   
    e=umount(mountpoint);
    if (e) {
	errmsg("failed to unmount: %s\n",strerror(errno));
	goto err;
    }
    remove_mtab(devname);

#if 0
    /* have to check whether user's own for this. */
    chown(mountpoint,0,0);   /* back to root */
    chmod(mountpoint,0700);  /* no public permissions */
#endif

    msg("disk unmounted");

    save_unlock_mtab();
    return 0;

err:
    save_unlock_mtab();
    return -1;
}

void list_drives() {
    mnt_node *mnt;
    int i,type;
    char devname[10];

    read_mtab();
/*
  printf("NAME  DEVICE     TYPE MOUNTPOINT STATUS\n");
  */
    printf("NAME   TYPE  STATUS\n");

    for(i=0;i<4;i++) {
	sprintf(devname,DEVICE_PATTERN,i);   
	type=probe_drive(devname);
	if (type) {
	    mnt=get_mounted(devname);
	    if (mnt) {
		printf(" " NAME_PATTERN "  %4dK  mounted on %s (%s %s)\n",
		       i,type,
		       mnt->ms.mnt_dir,
		       mnt->ms.mnt_type,
		       mnt->ms.mnt_opts);
	    }
	    else {
		printf(" " NAME_PATTERN "  %4dK  not mounted\n",
		       i,type);
	    }
	}
    }
}

int daemon_mode(char *devname,char *mountpoint,int mountflags,
                int interval,int drivetype) 
{
    int e,fd,disk_in,prev_disk_in,first;
    struct floppy_drive_struct state;
    mnt_node *mnt;

    strncpy(curdev,devname, sizeof(curdev));
    curdev[sizeof(curdev)-1]='\0';

    fd=open(devname,O_RDONLY|O_NDELAY);
    if (fd<0) {
	errmsg("error opening device: %s",strerror(errno));
	return -1;
    }

    prev_disk_in=0;
    first=1;

    ioctl(fd,FDFLUSH);
/*	close(fd);*/
    while(1) {
/*		fd=open(devname,O_RDONLY|O_NDELAY);
		if (fd<0) {
			errmsg("error opening device: %s",strerror(errno));
			return -1;
		}
		*/	
		usleep(interval*100000);
		e=ioctl(fd,FDPOLLDRVSTAT,&state);
		if (e) {
			msg("ioctl(FDPOLLDRVSTAT) failed: %s",strerror(errno));
			return -1;
		}
		printf("flags=%02lx\n", state.flags);
		disk_in=!(state.flags & (FD_VERIFY | FD_DISK_NEWCHANGE));
		if (disk_in && !prev_disk_in && !first) {
			msg("disk inserted");
			ioctl(fd,FDFLUSH);
			do_mount(devname,mountpoint,mountflags,0,1,drivetype);
		}
		if (!disk_in && prev_disk_in) {
			msg("disk removed");
			read_mtab();
			mnt=get_mounted(devname);
			if (mnt) {
				if (!hasmntopt(&mnt->ms,"sync") && !hasmntopt(&mnt->ms,"ro"))
					msg("arg!! wasn't mounted sync");
				/* should check for dirty buffers here! */
				do_umount(devname,0);
			}
			
		}
		prev_disk_in=disk_in;
		first=0;
/*		close(fd);*/
    }
}

void syntax() {
    fprintf(stderr,
	    "usage: fdmount [options] drive_name [mount_dir]\n"
	    "       fdumount [options] drive_name\n"
	    "       fdlist\n"
	    "       fdmountd [options] drive_name [mount_dir]\n"
	    "\n"
	    "    -r --readonly    mount read-only\n"
	    "    -s --sync        synchronize writes\n"
	    "       --nodev       ignore device flags\n"
	    "       --nosuid      ignore suid flags\n"
	    "       --noexec      ignore executable flags\n"
	    "    -f --force       force mount/unmount\n"
	    "    -l --list        list known drives\n"
	    "    -d --daemon      run as daemon\n"
	    "    -i --interval n  set probing interval for -d [0.1 seconds]\n"
	    "    -o --options l   sets filesystem specific options\n"
	    "       --silent      don't print informational messages\n"
	    "       --detach      run daemon in the background\n"
	    "       --nosync      don't mount synchronously, even if daemon\n"
	    "    -p --pidfile     dump the process id of the daemon to a file\n"
	    "    -v --vfat        use vfat fs, instead of msdos\n"
	    "    -h --help        this message\n\n");
    exit(1);
}

void dump_pid(char *name, int pid)
{
   FILE *fp;

   if(!name)
       return;
   fp=fopen(name, "w");
   if(!fp) {
       errmsg("Can't write pidfile\n");
       return;
   }
   fprintf(fp,"%d\n",pid);
   fclose(fp);
}

char *allowed_dos_options[]= {
    "check=r", "check=n", "check=s", "conv=", "dotsOK=", "debug", "fat=",
    "quiet", "blocksize=", 0 };

char *allowed_ext2_options[]= {
    "check=normal", "check=strict", "check=none", "errors=",
    "grpid", "bsdgroups", "nogrpid", "sysvgroups", "bsddf", "minixdf", 
    "resgid=", "debug", "nocheck", 0 };

int add_opt(char **allopts,
	    char *optlist, int *offset, char *new, int l)
{
    for(;*allopts; allopts++) {
	if(!strncmp(*allopts, new, strlen(*allopts))) {
	    if(l + *offset + 2> MAX_OPT)
		die("too many options");
	    optlist[(*offset)++] = ',';
	    strncpy(optlist + *offset, new, l);
	    *offset += l;
	    optlist[(*offset)] = '\0';
	    return 0;
	}
    }
    return -1;
}

static void add_opts(char *opts)
{
    char *newopt;
    int l;
    int dos_off, ext2_off;

    dos_off = strlen(dos_options);
    ext2_off = strlen(ext2_options);
    
    while(opts && *opts) {
	newopt = strchr(opts, ',');
	if(newopt) {
	    l = newopt - opts;
	    newopt++;
	} else
	    l = strlen(opts);
	
	if((add_opt(allowed_dos_options, dos_options, &dos_off, opts, l) &
	    add_opt(allowed_ext2_options, ext2_options, &ext2_off, opts, l))) {
	    opts[l]='\0';
	    die("Illegal option %s", opts);
	}
	opts = newopt;
    }    
}

int main(int argc, char **argv)
{
    int pid;
    int i,e,c,is_default,optidx=0;
    char *drivename,*mountpoint=NULL,def_mountpoint[40],devname[40];
    int drivetype;
    static int opt_force=0, opt_list=0, opt_daemon=0,
	opt_interval=10,opt_help=0,opt_umount=0,opt_nosync=0,
	opt_noexec=0,opt_nodev=0,opt_nosuid=0, opt_vfat=0;
    int mountflags=0;
    char *opt_pidfile="/var/run/fdmount.pid"; 
#ifdef FLOPPY_ONLY
    gid_t groups[NGROUPS_MAX];
    int not_allowed = 1, ngroups;
#endif                             

    static struct option longopt[] = {
	{ "silent",	0, &opt_silent,	1 },
	{ "detach",	0, &opt_detach,	1 },
	{ "readonly",	0, NULL,	'r' },
	{ "pidfile",	1, NULL,	'p' },
	{ "noexec",	0, &opt_noexec,	MS_NOEXEC },
	{ "nodev",	0, &opt_nodev,	MS_NODEV },
	{ "nosuid",	0, &opt_nosuid,	MS_NOSUID },
	{ "sync",	0, NULL,	's' },
	{ "nosync",	0, &opt_nosync,	1 },
	{ "force",	0, &opt_force,	1 },
	{ "list",	0, &opt_list,	1 },
	{ "daemon",	0, NULL,	'd' },
	{ "options",	0, NULL,	'o' },
	{ "interval",	1, NULL,	'i' },
	{ "vfat",	0, &opt_vfat,	'v' },
	{ "help",	0, &opt_help,	1 },
	{0}
    };

    progname = strrchr(argv[0], '/');
    if(progname)
	progname++;
    else
	progname = argv[0];

    opt_umount=(strcmp(progname,"fdumount")==0);
    opt_daemon=(strcmp(progname,"fdmountd")==0);
    opt_list  =(strcmp(progname,"fdlist")==0);

#ifdef FLOPPY_ONLY
    if ((ngroups = getgroups (NGROUPS_MAX, groups)) != -1) {
    	int     i;
    	struct group *gr;

		not_allowed = getuid();

        for (i = 0; not_allowed && i < ngroups; i++)
            if ((gr = getgrgid (groups[i])))
            	not_allowed = strcmp (gr -> gr_name, "floppy");
    }
    if (not_allowed)
        die("Must be member of group floppy");
#endif


    if (geteuid()!=0) 
	die("Must run with EUID=root");
    ruid = getuid();
    rgid = getgid();
    _umask = umask(0);
    umask(_umask);

    *dos_options = 0;
    *ext2_options = 0;

    while(1) {
	c=getopt_long(argc,argv,"rsfldi:hp:o:v",longopt,&optidx);
	if (c==-1) break;
	switch(c) {
	case 'o':
	    add_opts(optarg);
	    break;
	case 'r':
	    mountflags |= MS_RDONLY;
	    break;
	case 's':
	    mountflags |= MS_SYNCHRONOUS;
	    break;
	case 'l':
	    opt_list=1;
	    break;
	case 'f':
	    opt_force=1;
	    break;
	case 'd':
	    mountflags |= MS_SYNCHRONOUS;
	    opt_daemon=1;
	    break;
	case 'i':
	    opt_interval=atoi(optarg);
	    break;
	case 'p':
	    opt_pidfile = optarg;
	    break;
	case 'h':
	    opt_help=1;
	    break;
	case 'v': 
	    opt_vfat=1; 
	    break; 
	case 0:
	    break;
	default:
	    syntax();
	}
    }

    mountflags |= opt_noexec | opt_nosuid | opt_nodev;

    if (opt_vfat) {
	fsnames[T_DOS] = "vfat"; 
    }
    
    if(opt_nosync)
	mountflags &= ~MS_SYNCHRONOUS;
    
    if(opt_detach && !opt_daemon)
	die("Detach option can only be used when running as daemon\n");
    
    if(opt_force && ruid)
	die("You must be root to use the force option");

    drivename=argv[optind++];
    if (drivename) mountpoint=argv[optind++];
    if (optind<argc) syntax();
   
    if (!drivename) drivename="fd0";
   
    for(i=0;i<8;i++) {
	char dr[8];
	sprintf(dr,NAME_PATTERN,i);
	if (strcmp(dr,drivename)==0) {
	    sprintf(devname,DEVICE_PATTERN,i);
	    sprintf(def_mountpoint,DEFAULT_DIR_PATTERN,i);
	    break;
	}
    }
    if (i==8) {
	die("invalid drive name: %s\n"
            "use fd[0-7]\n",
            drivename);
    }
    if (!mountpoint) mountpoint=def_mountpoint;
    is_default=(strcmp(mountpoint,def_mountpoint)==0);

    if (opt_interval==0) opt_interval=10;
    if (opt_interval>10000) opt_interval=10000;

    /* get drive type */
    drivetype=probe_drive(devname);
    if (!drivetype) {
	die("drive %s does not exist",drivename);
    }

    e=0;
    if (opt_help) {
	syntax();
    }
    else if (opt_list) {
	list_drives();
    }
    else if (opt_daemon) {
	if (ruid!=0)
	    die("Must be root to run as daemon");

	if (opt_detach) {
	    pid = fork();
	    if (pid == -1)
		die("Failed to fork");
	    if (pid) {
		dump_pid(opt_pidfile, pid);
		exit(0); /* the father */
	    }
	    openlog(progname, 0, LOG_DAEMON); 
	    use_syslog = 1;

	    setsid();
	    chdir ("/"); /* no current directory. */
	    
	    /* Ensure that nothing of our device environment is inherited. */
	    close (0);
	    close (1);
	    close (2);      
	} else if (opt_pidfile)
	    dump_pid(opt_pidfile, getpid());
	daemon_mode(devname,mountpoint,mountflags,
		    opt_interval,drivetype);
    }
    else if (opt_umount) {
	e=do_umount(devname,opt_force);
    }
    else {
	if (ruid!=0) mountflags |= MS_NODEV|MS_NOSUID;
	e=do_mount(devname,mountpoint,mountflags,
		   opt_force,is_default,drivetype);
    }

    return e;
}
