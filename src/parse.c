#include <stdlib.h>
#include <string.h>
#include "parse.h"

int _set_int(char *name, keyword_t *ids, int size, int *mask)
{
	int i, slot;
	char *ptr;
	int *var;

	ptr = strchr(name,'=');
	if(ptr) {
		*ptr = '\0';
		ptr++;
		}
	
	for(i=0; i<size; i++) {
		if(!strcasecmp(ids[i].name, name)) {
			slot = ids[i].slot;
			var = ids[i].var;
			*mask |= 1 << slot;
			if(ptr) {
				*var = strtoul(ptr, &ptr,0);
				switch(*ptr) {
					case 'k':
					case 'K':
						*var *= 1024;
						break;
					case 'b':
						*var *= 512;
						break;
					default:
						break;
				}
			} else
				*var = ids[i].deflt;
			return 0;
			}
	}
	return 1;
}

void _zero_all(keyword_t *ids, int size, int *mask)
{
	int i, *var;

	*mask = 0;
	for(i=0; i<size; i++) {
		var = ids[i].var;
		*var = 0;
	}
}



