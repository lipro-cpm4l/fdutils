%{
#include <stdio.h>
#include <string.h>
#include "parse.h"
#include "driveprmP.h"
#include "driveprm.h"
static int lineno;
static int col;
%}

%option noyywrap

vid     [A-Za-z_][A-Za-z0-9_]*
number	(0x[a-zA-Z0-9]+|-?[0-9]+)(KB|s)?

%%
\n { col = 0; lineno++;}
[ \t]  col++;
#.*\n   { col = 0; lineno++;}
drive[0-7]: {
	col += yyleng;
        if(*found) return 0;
	_zero_all(ids, size, mask);

	yytext[yyleng-1]='\0';
	if(yytext[5] - '0' == drivenum)
		*found = 1;
}

3.5      |
5.25     |
8        |
ss       |
SS       |
ds       |
DS       |
sd       |
SD       |
dd       |
DD       |
qd       |
QD       |
hd       |
HD       |
ed       |
ED       |
{vid}={number} {
	col += yyleng;
	_set_int(yytext, ids, size, mask);
}

[^\t \n][^\t =\n]* {
	fprintf(stderr,
		"Syntax error in " DRIVEPRMFILE 
		" at line %d col %d: %s unexpected\n",
		lineno + 1, col + 1, yytext_ptr);
	return 1;
}
%%
