void mediaprm_match(char *name, int mode);
void mediaprm_init_search(char *name);
void mediaprm_stop_if_found(void);
int mediaprm_set_int(char *name);
void mediaprm_zero_all(void);

extern FILE *mediaprmin;

#define YY_DECL int mediaprmlex(char *name, struct keyword_l *ids, int size, \
			       int *mask, int *found)

YY_DECL;
