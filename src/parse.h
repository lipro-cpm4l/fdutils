#ifndef PARSE_H
#define PARSE_H

typedef struct keyword_l {
	char *name;
	int slot;
	int *var;
	int deflt;
} keyword_t;

int _set_int(char *name, keyword_t *ids, int size, int *mask);
void _zero_all(keyword_t *ids, int size, int *mask);



#define set_int(string, ids, mask)  \
_set_int(string, ids, sizeof(ids)/sizeof(ids[0]), mask);

#define zero_all(ids, mask)  \
_zero_all(ids, sizeof(ids)/sizeof(ids[0]), mask);

#endif
