/*   $Id: irr_lexer.y,v 1.1.1.1 2000/02/29 22:28:30 labovit Exp $
 * originally Id: irr_lexer.y,v 1.1 1998/01/24 00:51:20 labovit Exp 
 
  Copyright (c) 1994 by the University of Southern California
  and/or the International Business Machines Corporation.
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and
  its documentation in source and binary forms for lawful
  non-commercial purposes and without fee is hereby granted, provided
  that the above copyright notice appear in all copies and that both
  the copyright notice and this permission notice appear in supporting
  documentation, and that any documentation, advertising materials,
  and other materials related to such distribution and use acknowledge
  that the software was developed by the University of Southern
  California, Information Sciences Institute and/or the International
  Business Machines Corporation.  The name of the USC or IBM may not
  be used to endorse or promote products derived from this software
  without specific prior written permission.

  NEITHER THE UNIVERSITY OF SOUTHERN CALIFORNIA NOR INTERNATIONAL
  BUSINESS MACHINES CORPORATION MAKES ANY REPRESENTATIONS ABOUT
  THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND 
  NON-INFRINGEMENT.

  IN NO EVENT SHALL USC, IBM, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
  THE USE OR PERFORMANCE OF THIS SOFTWARE.

  Questions concerning this software should be directed to 
  info-ra@isi.edu.

  Author(s): Cengiz Alaettinoglu (cengiz@isi.edu) */




/* definitions */
%x USEFUL_LINE
%x USELESS_LINE
%x ASPATHRE
%x NONPOLICY_LINE

%{

#include "config.hh"
#include <iostream.h>
#include "dbase.hh"
#include "Node.h"
#include "irr_parser.h"
#include "trace.hh"

#define ERROR_LINE_LENGTH 512
#define yylval irrlval
#define LEXER_RETURN(x)  return(x)
#define YY_USER_ACTION    {                 \
      if (yylinebol) {                      \
	 *yyline = 0;                       \
	 yylineptr = yyline;                \
         yyerrstart  = 0;                   \
         yyerrlength = 0;                   \
      }                                     \
      if (yylineptr - yyline + yyleng + 1 < ERROR_LINE_LENGTH) { \
         strcpy(yylineptr, yytext);            \
         yylineptr += yyleng;                  \
      }\
      yylength = yyleng;                    \
      yylinebol = !strcmp(yytext, "\n");    \
   }

#include <cctype>

static inline strtoupper(char *c) {
   for (; *c; c++)
      if (isascii(*c) && isalpha(*c))
	 *c = toupper(*c);
}

char yyline[ERROR_LINE_LENGTH] = "";
int  yylinebol = 1;
int  yyerrstart  = 0;
int  yyerrlength = 0;
int  yylength = 0;
char *yylineptr;

int irr_lexer_input_size = 0;

int nonpolicy_prolog = 1;

typedef struct _KeyWord {
   char *val;
   int num;
} KeyWord;

static KeyWord keywords[] = {
"AND",         OP_AND,
"and",         OP_AND,
"OR",          OP_OR,
"or",          OP_OR,
"NOT",         OP_NOT,
"not",         OP_NOT,
"ANY",         KW_ANY,
"any",         KW_ANY,
"FROM",        KW_FROM,
"from",        KW_FROM,
"TO",          KW_TO,
"to",          KW_TO,
"ACCEPT",      KW_ACCEPT,
"accept",      KW_ACCEPT,
"ANNOUNCE",    KW_ANNOUNCE,
"announce",    KW_ANNOUNCE,
"METRIC-OUT",  KW_METRIC_OUT,
"metric-out",  KW_METRIC_OUT,
"DPA",         KW_DPA,
"dpa",         KW_DPA,
"IGP",         KW_IGP,
"igp",         KW_IGP,
"MED",         KW_MED,
"med",         KW_MED,
"PREF",        KW_PREF,
"pref",        KW_PREF,
"DEFAULT",     KW_DEFAULT,
"default",     KW_DEFAULT,
"STATIC",      KW_STATIC,
"static",      KW_STATIC,
NULL,          TKN_ERROR
};

static int get_keyword_num(char *string) {
   int i;

   for (i = 0; keywords[i].val; i++)
      if (!strcmp(keywords[i].val, string))
	 break;

   return(keywords[i].num);
}

typedef struct _Attribute_t {
   char *long_name;
   char *short_name;
   int  num;
} Attribute_t;

static Attribute_t useful_attrs[] = {
// attributes of aut-num object
"as-in:",                     "*ai:",   ATTR_AS_IN,
"as-out:",                    "*ao:",   ATTR_AS_OUT,
"interas-in:",                "*it:",   ATTR_INTERAS_IN,
"interas-out:",               "*io:",   ATTR_INTERAS_OUT,
"extended-as-in:",            "*xa:",   ATTR_EXT_AS_IN,
"extended-as-out:",           "*xb:",   ATTR_EXT_AS_OUT,
"extended-interas-in:",       "*xc:",   ATTR_EXT_INTERAS_IN,
"extended-interas-out:",      "*xd:",   ATTR_EXT_INTERAS_OUT,
"default:",                   "*df:",   ATTR_DEFAULT,
// attributes of route object
"route:",                     "*rt:",   ATTR_ROUTE,
"origin:",                    "*or:",   ATTR_ORIGIN,
"comm-list:",                 "*cl:",   ATTR_COMM_LIST,
NULL,                          NULL,    TKN_ERROR
};

static int get_attr_num(char *string) {
   int i;

   for (i = 0; useful_attrs[i].long_name; i++)
      if (!strcmp(useful_attrs[i].long_name, string)
	  || !strcmp(useful_attrs[i].short_name, string))
	 break;

  return(useful_attrs[i].num);
}

extern "C" {
int yywrap () {
   nonpolicy_prolog = 1;  // Reset to 1 if end-of-file encounters
   return 1;
}
}

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) { \
	result = MIN(max_size, irr_lexer_input_size); \
        if (((result = fread(buf, 1, result, yyin)) == 0) && ferror(yyin)) \
           YY_FATAL_ERROR( "input in flex scanner failed" ); \
	if (result < max_size) \
           buf[result] = 0; \
        irr_lexer_input_size -= result; \
        if (result) \
           Trace(TR_WHOIS_RESPONSE) << "Whois: Response " << buf << endl; \
}

%}

%%

%{
/* Rules */
%}

^[^ \t\n]+: {
   int token_type;

   if ((token_type = get_attr_num(yytext)) != TKN_ERROR) {
      nonpolicy_prolog = 0;
      BEGIN(USEFUL_LINE);
      LEXER_RETURN(token_type);
   } else {
      yymore();
      BEGIN(NONPOLICY_LINE);
   }
}

\n {
   /* simply skip this line, it is not interesting to us */
}

. {
   /* simply skip this line, it is not interesting to us */
   BEGIN(USELESS_LINE);
}

<USELESS_LINE>.* {
   /* simply skip this line, it is not interesting to us */
}

<USELESS_LINE>\n {
   /* simply skip this line, it is not interesting to us */
   BEGIN(INITIAL);
}

<NONPOLICY_LINE>.* {
   yylval.val = yytext; 
   if (nonpolicy_prolog) 
      LEXER_RETURN(NONPOLICY_PROLOG);
   else 
      LEXER_RETURN(NONPOLICY_EPILOG);
}

<NONPOLICY_LINE>\n {
   BEGIN(INITIAL);
   LEXER_RETURN((int) *yytext); 
}

<USEFUL_LINE>[a-z-]+ {
   LEXER_RETURN(get_keyword_num(yytext));
}

<ASPATHRE,USEFUL_LINE>[Aa][Ss][0-9]+ {
   strtoupper(yytext);

   yylval.pi = AS_map.add_entry(yytext);
   LEXER_RETURN(TKN_ASNUM);
}

<ASPATHRE,USEFUL_LINE>[Aa][Ss]-[A-Za-z0-9_-]+ {
   strtoupper(yytext);

   yylval.pi = ASMacro_map.add_entry(yytext);
   LEXER_RETURN(TKN_ASMACRO);
}

<USEFUL_LINE>[A-Za-z][A-Za-z0-9_-]* {
   int token_type;

   strtoupper(yytext);

   if ((token_type = get_keyword_num(yytext)) == TKN_ERROR) {
      token_type = TKN_CNAME;
      yylval.pi = Community_map.add_entry(yytext);
   }

   LEXER_RETURN(token_type);
}

<USEFUL_LINE>[0-9]+(\.[0-9]+){3,3}\/[0-9]+ {
   yylval.pi = Prefask_map.add_entry(yytext);
   LEXER_RETURN(TKN_PRFMSK);
}

<USEFUL_LINE>[0-9]+(\.[0-9]+){3,3} {
   yylval.val = yytext;
   LEXER_RETURN(TKN_ADDRESS);
}

<USEFUL_LINE>[0-9]+ {
   yylval.i = atoi(yytext);
   LEXER_RETURN(TKN_NUM);
}

<USEFUL_LINE>[,{}()=;] { 
   LEXER_RETURN((int) *yytext); 
}

<USEFUL_LINE>\< {
   BEGIN(ASPATHRE);
   LEXER_RETURN((int) *yytext); 
}

<ASPATHRE>\> {
   BEGIN(USEFUL_LINE);
   LEXER_RETURN((int) *yytext); 
}

<ASPATHRE>[()^|*+\-\.[\]?$] { 
   LEXER_RETURN((int) *yytext); 
}

<ASPATHRE,USEFUL_LINE>[ \t]+ { 
   /* Skip white space */
}

<ASPATHRE>_+ { 
   /* Skip white space */
}

<ASPATHRE,USEFUL_LINE>\n {
   BEGIN(INITIAL);
   LEXER_RETURN((int) *yytext); 
}

<ASPATHRE,USEFUL_LINE>. {
   LEXER_RETURN(TKN_ERROR);
}
%%

/* User Code if any */

