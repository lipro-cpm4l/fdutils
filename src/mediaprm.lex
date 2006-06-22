%{
#include <stdio.h>
#include <string.h>
#include "parse.h"
#include "mediaprmP.h"
#include "mediaprm.h"
static int lineno;
static int col;
%}

%option noyywrap
%option pointer

fid	[^\"]+
vid     [A-Za-z_][A-Za-z0-9_-]*
number	(0x[a-zA-Z0-9]+|-?[0-9]+)(KB|k|b)?

%%
\n { col = 0; lineno++; }
[ \t] { col++;}
#.*\n   { col = 0; lineno++;}
\042{fid}\042: {
	col += yyleng;
	if(*found) return 0;
	_zero_all(ids, size, mask);
	
	yytext[yyleng-2]='\0';
	if(!strcasecmp(yytext+1, name))
		*found = 1;
}

alias=\042{fid}\042 {
	col += yyleng;
	yytext[yyleng-1]='\0';
	if(!strcasecmp(yytext+7, name))
		*found = 1;
}

swapsides |
zerobased |
zero-based |
mss      |
2m	 |
2M	 |
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
	yytext[yyleng-1]='\0';
	fprintf(stderr,
		"Syntax error in " MEDIAPRMFILE
		" at line %d col %d: %s unexpected\n",
		lineno+1, col+1, yytext_ptr);
	return 1;
}
%%

