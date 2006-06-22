#ifndef __FDUTILS_H
#define __FDUTILS_H

#include <assert.h>
/* This file contains common structures understood by several of the
 * fdutils
 */

/* format map */
typedef	struct format_map {
	unsigned char cylinder;
	unsigned char head;
	unsigned char sector;
	unsigned char size;
} format_map_t;

void readid(int fd, int dn, int rate, int cylinder);
int measure_raw_capacity(int fd, int dn, int rate,
			int cylinder, int warmup, int verbosity);

#define NewArray(n,type) ((type *)(calloc((n), sizeof(type))))
#define New(type) ((type *)(malloc(sizeof(type))))

#define SafeNewArray(n,type) ((type *)(safe_calloc((n), sizeof(type))))
#define SafeNew(type) ((type *)(safe_malloc(sizeof(type))))
void *safe_malloc(size_t size);
void *safe_calloc(size_t nmemb, size_t size);

#ifndef FD_SWAPSIDES
#define FD_SWAPSIDES 2
#endif

#ifndef FD_ZEROBASED
#define FD_ZEROBASED 4
#endif

#endif
