#include <stdio.h>
#include <string.h>
#include <stdlib.h>


FILE *my_open(char *directory, char *file)
{
	char path[8192];
	FILE *r;

	strcpy(path, directory);
	strcat(path,"/");
	strcat(path, file);
	
	r= fopen(path, "r");
	if(r == NULL) {
		fprintf(stderr,"Could not open %s\n", path);
		perror("open");
		exit(1);
	}
	return r;
}

int main(int argc, char **argv)
{
  FILE *stack[256];
  int sp;
  char line[4096];
  char *directory;

  sp = 0;

  if(argc < 3) {
	  fprintf(stderr,"Usage: %s directory root texi file\n", argv[0]);
	  exit(1);
  }

  directory = argv[1];

  stack[sp++] = my_open(directory, argv[2]);
  while(sp) {
	  if(fgets(line, 4095, stack[sp-1]) ) {
		  line[strlen(line)-1]='\0';
		  if(!strncmp(line, "@input", 6)) {
			  stack[sp++] = my_open(directory, line+7);
			  continue;
		  }
		  if(!strncmp(line, "@include", 8)) {
			  stack[sp++] = my_open(directory, line+9);
			  continue;
		  }
		  puts(line);
	  } else
		  sp--;
  }
  exit(0);
}
