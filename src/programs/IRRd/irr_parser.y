%{
// 
//  Copyright (c) 1994 by the University of Southern California
//  and/or the International Business Machines Corporation.
//  All rights reserved.
//
//  Permission to use, copy, modify, and distribute this software and
//  its documentation in source and binary forms for lawful
//  non-commercial purposes and without fee is hereby granted, provided
//  that the above copyright notice appear in all copies and that both
//  the copyright notice and this permission notice appear in supporting
//  documentation, and that any documentation, advertising materials,
//  and other materials related to such distribution and use acknowledge
//  that the software was developed by the University of Southern
//  California, Information Sciences Institute and/or the International
//  Business Machines Corporation.  The name of the USC or IBM may not
//  be used to endorse or promote products derived from this software
//  without specific prior written permission.
//
//  NEITHER THE UNIVERSITY OF SOUTHERN CALIFORNIA NOR INTERNATIONAL
//  BUSINESS MACHINES CORPORATION MAKES ANY REPRESENTATIONS ABOUT
//  THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
//  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
//  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
//  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND 
//  NON-INFRINGEMENT.
//
//  IN NO EVENT SHALL USC, IBM, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
//  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
//  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
//  THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
//  Questions concerning this software should be directed to 
//  info-ra@isi.edu.
//
//  Author(s): Cengiz Alaettinoglu (cengiz@isi.edu)

/*
 *[C] The Regents of the University of Michigan and Merit Network, Inc.1993 
 *All Rights Reserved 
 *  
 *  Permission to use, copy, modify, and distribute this software and its 
 *  documentation for any purpose and without fee is hereby granted, provided 
 *  that the above copyright notice and this permission notice appear in all 
 *  copies of the software and derivative works or modified versions thereof, 
 *  and that both the copyright notice and this permission and disclaimer 
 *  notice appear in supporting documentation. 
 *   
 *   THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER 
 *   EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION WARRANTIES OF 
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE REGENTS OF THE 
 *   UNIVERSITY OF MICHIGAN AND MERIT NETWORK, INC. DO NOT WARRANT THAT THE 
 *   FUNCTIONS CONTAINED IN THE SOFTWARE WILL MEET LICENSEE'S REQUIREMENTS OR 
 *   THAT OPERATION WILL BE UNINTERRUPTED OR ERROR FREE. The Regents of the 
 *   University of Michigan and Merit Network, Inc. shall not be liable for any 
 *   special, indirect, incidental or consequential damages with respect to any 
 *   claim by Licensee or any third party arising from use of the software. 
 */

#include "config.hh"

#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdarg>

extern "C" {
#if HAVE_MEMORY_H && 0
#   include <memory.h>
#endif
#ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#endif
}

#include "Node.h"
#include "dbase.hh"
#include "aut-num.hh"
#include "Route.hh"
#include "regexp.hh"

AutNum *irr_parser_as = NULL;
Route  *irr_parser_rt = NULL;
int irr_ignore_comm_list = 0;

int irr_parser_had_errors = 0;

extern int yyerror(char *);
extern int yylex();
char *error_while_expecting = NULL;

static regexp_symbol *re_symbol;
static insert_or_update_filter_action(ListHead<Filter_Action> &l, 
			    ActionNode *action, FilterNode *filter);
static ASPolicy *find_or_insert_peer_as(Pix peer);
static InterASPolicy *find_or_insert_peering(ASPolicy *p, Pix laddr, Pix raddr);

static WhoisParser parser_dbc;

static int count = 0;

%}

%union {
   int i;
   char *val;
   FilterNode *node_ptr;
   ActionNode *action;
   NetListNode *netlistnode_ptr;
   regexp *re;
   Pix pi;
}

%token <pi>  TKN_PRFMSK      501
%token <i>   TKN_NUM         502
%token <pi>  TKN_ASNUM       503
%token <pi>  TKN_ASMACRO     504
%token <pi>  TKN_CNAME       505
%token <val> TKN_ERROR       506
%token <val> TKN_ADDRESS     507

%token <val> KW_FROM         300
%token <val> KW_ACCEPT       301
%token <val> KW_TO           302
%token <val> KW_ANNOUNCE     303
%token <val> KW_PREF         304
%token <val> KW_MED          305
%token <val> KW_METRIC_OUT   306
%token <val> KW_IGP          307
%token <val> KW_ANY          308
%token <val> KW_DEFAULT      309
%token <val> KW_STATIC       310
%token <val> KW_DPA          311

%left  <val> OP_OR           600
%left  <val> OP_AND          601
%right <val> OP_NOT          602

%token <val> ATTR_AS_IN           400
%token <val> ATTR_AS_OUT          401
%token <val> ATTR_INTERAS_IN      402
%token <val> ATTR_INTERAS_OUT     403
%token <val> ATTR_EXT_AS_IN       404
%token <val> ATTR_EXT_AS_OUT      405
%token <val> ATTR_EXT_INTERAS_IN  406
%token <val> ATTR_EXT_INTERAS_OUT 407
%token <val> ATTR_ROUTE           408
%token <val> ATTR_ORIGIN          409
%token <val> ATTR_COMM_LIST       410
%token <val> ATTR_DEFAULT         411

%token <val> NONPOLICY_PROLOG     700
%token <val> NONPOLICY_EPILOG     701

%type <node_ptr> rpe
%type <node_ptr> rpe_term
%type <node_ptr> rpe_factor
%type <node_ptr> operand
%type <node_ptr> default_line_exp
%type <netlistnode_ptr> netlist
%type <i> input
%type <i> input_line
%type <i> policy_line
%type <i> nonpolicy_line
%type <i> as_in_line 
%type <i> as_out_line
%type <i> default_line
%type <i> interas_in_line
%type <i> interas_out_line
%type <i> rt_origin_line
%type <i> rt_route_line
%type <i> rt_comm_list_line
%type <action> as_in_action
%type <action> interas_in_action
%type <action> interas_out_action
%type <action> interas_out_multi_action
%type <action> interas_out_one_action
%type <i> ext_as_in_line
%type <i> ext_as_out_line
%type <i> ext_interas_in_line
%type <i> ext_interas_out_line
%type <re> aspathexp
%type <re> aspathre
%type <re> aspathre_term
%type <re> aspathre_closure
%type <re> aspathre_factor
%type <re> aspathre_no
%type <re> aspathre_range
%type <re> aspathre_subrange
%type <i> opt_from
%type <i> opt_accept
%type <i> opt_to
%type <i> opt_announce
%type <pi> address
%start input
%%

input: input_line
|
input input_line
;

input_line: policy_line '\n'
{
#ifdef VERBOSE_PARSER
   extern char yyline[];
   whois.error.information("%d:%s", count++, yyline);
#endif // VERBOSE_PARSER
}
| nonpolicy_line '\n'
{
#ifdef VERBOSE_PARSER
   extern char yyline[];
   whois.error.information("%d:%s", count++, yyline);
#endif // VERBOSE_PARSER
}
| '\n'
{ // empty line
#ifdef VERBOSE_PARSER
   extern char yyline[];
   whois.error.information("%d:%s", count++, yyline);
#endif // VERBOSE_PARSER
}
| error  '\n'
{
   count++;

   extern char yyline[];
   extern int  yyerrstart;
   extern int  yyerrlength;
   int i;

   char pointer[yyerrstart+yyerrlength+1];

   for (i = 0; i < yyerrstart; i++)
      pointer[i] = ' ';
   for (; i < yyerrlength + yyerrstart; i++)
      pointer[i] = '^';
   pointer[i] = 0;

   if (error_while_expecting)
      whois.error.error("Error: while parsing %s\nError:               %s\nError: %s expected.\n", 
			(char *) yyline, pointer, error_while_expecting);
   else
      whois.error.error("Error: while parsing %s\nError:               %s\n", 
			(char *) yyline, pointer);

   error_while_expecting = NULL;
   irr_parser_had_errors = 1;
}
;

policy_line: as_in_line 
| as_out_line
| interas_in_line
| interas_out_line
| default_line
| ext_as_in_line
| ext_as_out_line
| ext_interas_in_line
| ext_interas_out_line
| rt_origin_line
| rt_route_line
| rt_comm_list_line
;

as_in_line: ATTR_AS_IN opt_from TKN_ASNUM as_in_action opt_accept rpe {
   ASPolicy *p = find_or_insert_peer_as($3);   
   insert_or_update_filter_action(p->in, $4, $6);
}
;

opt_from: KW_FROM {} | {};
opt_accept: KW_ACCEPT {} | {};

as_in_action: TKN_NUM {
   $$ = new PrefNode($1);
};

as_out_line: ATTR_AS_OUT opt_to TKN_ASNUM opt_announce rpe {
   ASPolicy *p = find_or_insert_peer_as($3);   
   insert_or_update_filter_action(p->out, new NoopNode, $5);
};

opt_to: KW_TO {} | {};
opt_announce: KW_ANNOUNCE {} | {};

interas_in_line: ATTR_INTERAS_IN opt_from TKN_ASNUM address address 
                    interas_in_action opt_accept rpe {
   ASPolicy *p = find_or_insert_peer_as($3);   
   InterASPolicy *ip = find_or_insert_peering(p, $4, $5);
   insert_or_update_filter_action(ip->in, $6, $8);
}
;

address: 
{
  error_while_expecting = "address like 128.9.128.9/32";
  yyerrok;
} TKN_PRFMSK {
   error_while_expecting = NULL;
   $$ = $2;
}
;

interas_in_action: '(' KW_PREF '=' TKN_NUM ')' {
   $$ = new PrefNode($4);
}
| '(' KW_PREF '=' KW_MED ')' {
   $$ = new PrefNode(-1);
};

interas_out_line: ATTR_INTERAS_OUT opt_to TKN_ASNUM address address 
                     interas_out_action opt_announce rpe {
   ASPolicy *p = find_or_insert_peer_as($3);   
   InterASPolicy *ip = find_or_insert_peering(p, $4, $5);
   insert_or_update_filter_action(ip->out, $6, $8);
}
;

interas_out_action: '(' interas_out_multi_action ')' {
   $$ = $2;
}
| {
   $$ = new NoopNode;
}
;

interas_out_multi_action: interas_out_one_action {
   $$ = $1;
}
| interas_out_multi_action ';' interas_out_one_action {
   $$ = new ComposeNode($1, $3);
}
;

interas_out_one_action: KW_METRIC_OUT '=' TKN_NUM {
   $$ = new MEDNode($3);
}
| KW_METRIC_OUT '=' KW_IGP  {
   $$ = new MEDNode(-1);
}
| KW_DPA '=' TKN_NUM {
   $$ = new DPANode($3);
}
;

default_line: ATTR_DEFAULT TKN_ASNUM as_in_action default_line_exp {
   ASPolicy *p = find_or_insert_peer_as($2); 
   insert_or_update_filter_action(p->dflt, $3, $4);
}
;

default_line_exp: /* empty */ {
   $$ = new ANYNode;
}
| KW_DEFAULT {
   $$ = new ANYNode;
}
| KW_STATIC {
   $$ = new ANYNode;
}
| rpe {
   $$ = $1;
}
;

ext_as_in_line: ATTR_EXT_AS_IN opt_from TKN_ASNUM as_in_action opt_accept rpe {
   ASPolicy *p = find_or_insert_peer_as($3);   
   insert_or_update_filter_action(p->ext_in, $4, $6);
}
;

ext_as_out_line: ATTR_EXT_AS_OUT opt_to TKN_ASNUM opt_announce rpe {
   ASPolicy *p = find_or_insert_peer_as($3);   
   insert_or_update_filter_action(p->ext_out, new NoopNode, $5);
};

ext_interas_in_line: ATTR_EXT_INTERAS_IN opt_from TKN_ASNUM address address 
                    interas_in_action opt_accept rpe {
   ASPolicy *p = find_or_insert_peer_as($3);   
   InterASPolicy *ip = find_or_insert_peering(p, $4, $5);
   insert_or_update_filter_action(ip->ext_in, $6, $8);
}
;

ext_interas_out_line: ATTR_EXT_INTERAS_OUT opt_to TKN_ASNUM address address 
                     interas_out_action opt_announce rpe {
   ASPolicy *p = find_or_insert_peer_as($3);   
   InterASPolicy *ip = find_or_insert_peering(p, $4, $5);
   insert_or_update_filter_action(ip->ext_out, $6, $8);

}
;

rpe: rpe OP_OR rpe_term
{
   $$ = new ORNode($1, $3);
}
| rpe rpe_term %prec OP_OR
{
   $$ = new ORNode($1, $2);
}
| rpe_term
;

rpe_term : rpe_term OP_AND rpe_factor
{
   $$ = new ANDNode($1, $3);
}
|
rpe_factor
;

rpe_factor :  OP_NOT rpe_factor
{
   $$ = new NotNode($2);
}
| '(' rpe ')'
{
   $$ = $2;
}
| operand 
;

operand: TKN_ASNUM
{
   $$ = new ASNode( $1 );
}
| TKN_ASMACRO
{
   $$ = new ASMacroNode( $1 );
}
| TKN_CNAME
{
   $$ = new CommNode( $1 );
}
| KW_ANY
{
   $$ = new ANYNode;
}
| '{' netlist '}' 
{ 
   $$ = $2; 
}
| aspathexp
{
   $$ = new ASPathNode( $1 );
}
;

netlist: TKN_PRFMSK
{
   $$ = new NetListNode;
   $$->add($1);
}
| netlist ',' TKN_PRFMSK
{
   ($$ = $1)->add($3);
}
;

aspathexp:  '<' aspathre '>' {
   $$ = $2;
} 
;

aspathre: aspathre '|' aspathre_term {
   $$ = new regexp_or($1, $3);
}
| aspathre_term
;

aspathre_term: aspathre_term aspathre_closure {
   $$ = new regexp_cat($1, $2);
}
| aspathre_closure
;

aspathre_closure: aspathre_closure '*' {
   $$ = new regexp_star($1);
}
| aspathre_closure '?' {
   $$ = new regexp_question($1);
}
| aspathre_closure '+' {
   $$ = new regexp_plus($1);
}
| aspathre_factor
;

aspathre_factor: aspathre_no {
   $$ = $1;
}
| '^' {
   $$ = new regexp_bol();
}
| '$' {
   $$ = new regexp_eol();
}
| '(' aspathre ')' {
   $$ = $2;
}
;

aspathre_no: TKN_ASNUM {
   $$ = new regexp_symbol(REGEXP_SYMBOL_AS_NO, $1);
}
| '.' {
   $$ = new regexp_symbol(REGEXP_SYMBOL_ANY);
}
| '[' aspathre_range ']' {
   $$ = $2;
}
| '[' '^' aspathre_range ']' {
   $$ = $3;
   ((regexp_symbol *) $$)->complement();
}
| TKN_ASMACRO {
   $$ = new regexp_symbol(REGEXP_SYMBOL_AS_MACRO, $1);
}
;

aspathre_range: {
   $$ = re_symbol = new regexp_symbol;
}
| aspathre_range aspathre_subrange {
   $$ = $1;
}
;

aspathre_subrange: TKN_ASNUM {
   re_symbol->add(REGEXP_SYMBOL_AS_NO, $1);
}
| '.' {
   re_symbol->add(REGEXP_SYMBOL_ANY);
}
| TKN_ASNUM '-' TKN_ASNUM {
   re_symbol->add(REGEXP_SYMBOL_AS_NO, $1, $3);
}
| TKN_ASMACRO {
   re_symbol->add(REGEXP_SYMBOL_AS_MACRO, $1);
}
;

rt_route_line: ATTR_ROUTE TKN_PRFMSK {
   irr_parser_rt->nlri.pix = $2;
}
;

rt_origin_line: ATTR_ORIGIN TKN_ASNUM {
   ListNodePix *p;
   for (p = irr_parser_rt->origin.head(); 
	p; 
	p = irr_parser_rt->origin.next(p->l))
      if (p->pix == $2)
	 break;

   if (!p) {
      p = new ListNodePix;
      p->pix = $2;
      irr_parser_rt->origin.append(p->l);
      irr_ignore_comm_list = 0;
   } else
      irr_ignore_comm_list = 1;
}
;

rt_comm_list_line: ATTR_COMM_LIST rt_comm_list {
}
;

rt_comm_list: TKN_CNAME {
   if (!irr_ignore_comm_list) {
      ListNodePix *p;
      for (p = irr_parser_rt->ripecommunity.head(); 
	   p; 
	   p = irr_parser_rt->ripecommunity.next(p->l))
	 if (p->pix == $1)
	    break;
      
      if (!p) {
	 p = new ListNodePix;
	 p->pix = $1;
	 irr_parser_rt->ripecommunity.append(p->l);
      }
   }
}
| rt_comm_list TKN_CNAME {
   if (!irr_ignore_comm_list) {
      ListNodePix *p;
      for (p = irr_parser_rt->ripecommunity.head(); 
	   p; 
	   p = irr_parser_rt->ripecommunity.next(p->l))
	 if (p->pix == $2)
	    break;
      
      if (!p) {
	 p = new ListNodePix;
	 p->pix = $2;
	 irr_parser_rt->ripecommunity.append(p->l);
      }
   }
}
;

nonpolicy_line : NONPOLICY_PROLOG {
   if (irr_parser_as) {
      NonPolicyLines *line = new NonPolicyLines(strdup($1));
      irr_parser_as->prolog.append(line->node);
   }
}
               | NONPOLICY_EPILOG {
   if (irr_parser_as) {
      NonPolicyLines *line = new NonPolicyLines(strdup($1));
      irr_parser_as->epilog.append(line->node);
   }
}
;  


%%

extern char *yytext;

int yyerror(char *s) {
   extern char yyline[];
   extern int  yyerrstart;
   extern int  yyerrlength;
   extern char *yylineptr;
   extern int yylength;

   yyerrstart = yylineptr - yyline - yylength;
   yyerrlength = yylength;
   return(0);
}

static insert_or_update_filter_action(ListHead<Filter_Action> &l, 
				      ActionNode *action, FilterNode *filter) {
   Filter_Action *fap, *fap2;

   for (fap = l.head(); 
	fap && *fap->action < *action; 
	fap = l.next(fap->falist))
      ;

   if (!fap) {
      fap = new Filter_Action;
      fap->action = action;
      fap->filter = filter;
      l.append(fap->falist);
   } else 
      if (*fap->action == *action) {
	 fap->filter = new ORNode(fap->filter, filter);
	 delete action;
      } else {
	 fap2 = new Filter_Action;
	 fap2->action = action;
	 fap2->filter = filter;
	 l.insert_before(fap->falist, fap2->falist);
      }
}
  
static ASPolicy *find_or_insert_peer_as(Pix peer) {
   ASPolicy *p;
   if (!(p = irr_parser_as->find_peer(peer))) {
      p = new ASPolicy;
      p->peeras = peer;
      irr_parser_as->peers.append(p->peers);
   }
   return p;
}

static InterASPolicy *find_or_insert_peering(ASPolicy *p, 
					     Pix laddr, Pix raddr) {
   InterASPolicy *ip;
   if (!(ip = p->find_peering(laddr, raddr))) {
      ip = new InterASPolicy;
      ip->laddress = laddr;
      ip->raddress = raddr;
      p->interas.append(ip->interas);
   }
   return ip;
}
