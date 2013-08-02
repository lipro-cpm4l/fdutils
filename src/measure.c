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

static struct {
	unsigned char value;
	int offset;
} byte_tab[16]= {
	{ 0x03, 0 },
	{ 0x7c, 0 },
	{ 0x81, 0 },
	{ 0x3e, 0 },
	{ 0xc0, 0 },
	{ 0x1f, 0 },
	{ 0x60, 0 },
	{ 0x8f, 1 },
	{ 0x30, 1 },
	{ 0xc7, 1 },
	{ 0x18, 1 },
	{ 0xe3, 1 },
	{ 0x0c, 1 },
	{ 0xf1, 1 },
	{ 0x06, 1 },
	{ 0xf8, 1 }
};

static int read_track(int fd, int dn, int rate, int cylinder)
{
	struct floppy_raw_cmd raw_cmd;
	int tmp;
	unsigned char ref;

	unsigned char buffer[32*1024];
	unsigned char c;
	int ptr;
	int i;
	
	raw_cmd.cmd_count = 9; 
	raw_cmd.cmd[0] = FD_READ & ~0x80; /* format command */
	raw_cmd.cmd[1] = dn /* drive */;
	raw_cmd.cmd[2] = 0; /* cylinder */
	raw_cmd.cmd[3] = 0; /* head */
	raw_cmd.cmd[4] = 1; /* sector */
	raw_cmd.cmd[5] = 8; /* sizecode */
	raw_cmd.cmd[6] = 1; /* sect per track */
	raw_cmd.cmd[7] = 0x1b; /* gap */
	raw_cmd.cmd[8] = 0xff; /* sizecode2 */
	raw_cmd.data = buffer;
	raw_cmd.length = 32 * 1024;
	raw_cmd.flags = FD_RAW_INTR | FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK |
		FD_RAW_READ;
	raw_cmd.rate = rate;
	raw_cmd.track = cylinder;
	tmp = ioctl(fd, FDRAWCMD, & raw_cmd);
	if ( tmp < 0 ){
		perror("read");
		exit(1);
	}


	if((raw_cmd.reply[1] & ~0x20) |
	   (raw_cmd.reply[2] & ~0x20) |
	   raw_cmd.reply[3]) {
		int i;
		fprintf(stderr, "\nFatal error while measuring raw capacity\n");
		for(i=0; i < raw_cmd.reply_count; i++) {
			fprintf(stderr, "%d: %02x\n", i, raw_cmd.reply[i]);
		}
		exit(1);
	}

	ptr = 514;
	/* we look first for the place where the 0x4e's are going to stop */
	ref = buffer[ptr];
	while(buffer[ptr] == ref)
		ptr += 256;
	ptr -= 240;
	while(buffer[ptr] == ref)
		ptr += 16;
	ptr -= 15;
	while(buffer[ptr] == ref)
		ptr ++;
	/* we have now found the first byte after wrap-around */

	/* jump immediately to the supposed beginning */
	ptr += 210;
	c = buffer[ptr];
	while(buffer[ptr] == c)
		ptr--;


	for(i=0; i<16; i++) {
		if(byte_tab[i].value == c) {
			ptr -= byte_tab[i].offset;
			return ptr * 16 + i;
		}
	}
	return ptr * 16;
}


static void m_format_track(int fd, int dn, int rate, int cylinder)
{
	struct floppy_raw_cmd raw_cmd;
	int tmp;
	format_map_t format_map[1] = {
		{ 0, 0, 1, 8 }
	};
	
	raw_cmd.cmd_count = 6; 
	raw_cmd.cmd[0] = FD_FORMAT; /* format command */
	raw_cmd.cmd[1] = dn /* drive */;
	raw_cmd.cmd[2] = 2; /* sizecode used for formatting */
	raw_cmd.cmd[3] = 1; /* sectors per track */
	raw_cmd.cmd[4] = 255; /* gap. unimportant anyways */
	raw_cmd.cmd[5] = 3;
	raw_cmd.data = format_map;
	raw_cmd.length = sizeof(format_map);
	raw_cmd.flags = FD_RAW_INTR | FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK |
		FD_RAW_WRITE;
	raw_cmd.rate = rate;
	raw_cmd.track = cylinder;

	tmp = ioctl(fd, FDRAWCMD, & raw_cmd);
	if ( tmp < 0 ){
		perror("format");
		exit(1);
	}

	if((raw_cmd.reply[1] & ~0x20) |
	   (raw_cmd.reply[2] & ~0x20)) {
		int i;

		if ( raw_cmd.reply[1] & ST1_WP ){
		    fprintf(stderr,"The disk is write protected\n");
		    exit(1);
		}

		fprintf(stderr, 
			"\nFatal error while measuring raw capacity\n");
		for(i=0; i < raw_cmd.reply_count; i++) {
			fprintf(stderr, "%d: %02x\n", i, raw_cmd.reply[i]);
		}
		exit(1);
	}
}


void readid(int fd, int dn, int rate, int cylinder)
{
	struct floppy_raw_cmd raw_cmd;
	int tmp;

	raw_cmd.cmd_count = 2; 
	raw_cmd.cmd[0] = FD_READID; /* format command */
	raw_cmd.cmd[1] = dn /* drive */;
	raw_cmd.flags = FD_RAW_INTR | FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK;
	raw_cmd.rate = rate;
	raw_cmd.track = cylinder;
	
	tmp = ioctl( fd, FDRAWCMD, & raw_cmd );
	if ( tmp < 0 ){
		perror("readid");
		exit(1);
	}       
}


int measure_raw_capacity(int fd, int dn, int rate, 
			int cylinder, int warmup, int verbosity)
{
	int cap, min, ctr;

	if(warmup)
		/* first formatting, so that we have sth to read during the
		 * warmup cycles */
		m_format_track(fd, dn, rate, cylinder);

	m_format_track(fd, dn, rate, cylinder);
	min = cap = read_track(fd, dn, rate, cylinder);
	ctr = 0;
	while(ctr < 10) {
		m_format_track(fd, dn, rate, cylinder);
		cap = read_track(fd, dn, rate, cylinder);
		if(cap < min) {
			min = cap;
			ctr = 0;
		} else
			ctr++;
		if(verbosity)
			fprintf(stderr,"warmup cycle: %3d %6d %6d\r", ctr, 
				cap, min);

	}
	if(verbosity)
		fprintf(stderr,"                                \r");
	return min;
}
