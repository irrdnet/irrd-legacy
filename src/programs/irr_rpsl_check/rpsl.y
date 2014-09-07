/* 
 * $Id: rpsl.y,v 1.28 2002/10/17 20:22:10 ljb Exp $
 * originally Id: ripe181.y,v 1.19 1998/08/02 03:45:26 gerald Exp 
 */

%{

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <irr_rpsl_check.h>
#include <irr_defs.h>

#define YYDEBUG 1

/* defined in irr_attrs.c */

extern char *obj_type[MAX_OBJS];
extern short legal_attrs[MAX_OBJS][MAX_ATTRS]; 
extern canon_line_t lineptr[];
extern int QUICK_CHECK_FLAG;
extern FILE *dfile;
extern int ERROR_STATE;
extern u_int ri_opts;
char *p, *q;
static int i, mp_attr, via_attr, filter_count, import_errors, all, none;
extern int lncont;

/* defined in lexer rpsl.fl */
extern int tokenpos;

/* function prototypes */

void yyerror (char *);
int yyparse ();
int yylex ();
void start_new_attr ();

/* data struct that keeps info on the current object we are parsing */

parse_info_t curr_obj;
extern canon_info_t canon_obj;

/* RPSL Dictionary linked list's */

type_t_ll    type_ll;
rp_attr_t_ll rp_attr_ll;
proto_t_ll   proto_ll;
afi_t_ll     afi_ll;

/* scratch buffer to build error msg's */
#define ERR_BUF_SIZE 4096
char err_buf[ERR_BUF_SIZE];

/* dictionary var's */
rp_attr_t  *rp;
method_t   *method;
proto_t    *pr;

%}

%union {
  char *string;
  int attr;
  struct _param_t  *param;
  struct _type_t   *type;
  struct _method_t *method;
  struct _ph_t     *ph;
}

/* All non-field id's/attrs tokens */
%token T_NEWLINE
%token T_EOF 
%token T_JUNK T_COOKIE
%token <string> T_AS T_PREFIX T_PREFIX6 T_SPREFIX T_SPREFIX6
%token <string> T_BLOB T_COMMENT T_NUM T_REAL
%token <string> T_NON_NULL_LINE
%token <string> T_EMAIL T_ASNAME 
%token <string> T_PREFIXRNG T_PREFIXRNG6 T_DNAME
%token <string> T_RSNAME T_FLTRNAME T_RTRSNAME T_PRNGNAME
%token <string> T_WORD T_RP_ATTR T_OP T_MANDATORY T_OPTIONAL
%token <string> T_KCHEXID T_BEGIN_PGP_BLOCK T_VERSION
%token <string> T_END_PGP_BLOCK T_RADIX64
%token <string> T_TUNNEL T_GRE T_IPINIP
%token <string> T_MAILFROM T_CRYPTPW T_MD5PW T_NONE T_PGPKEY T_MAILTO
%token <string> T_WHOIS T_RPSQUERY T_FTP T_DIRPATH T_REFER_KEYWORD

%type <string> spec_attr /* ie, delete, password, override */
%type <string> free_text machine_generated

/* rpsl */

%type <string> prefix_types prefix_list sprefix_list
%type <string> mb_list email_list rt_member_of peer_option peer_options
%type <string> an_member_of ir_member_of peer_id opt_peer_options host_name
%type <string> mp_peer_id

%type <string> non_null_lines auth_line 
%type <string> opt_as_expression as_expression as_expression_term 
%type <string> as_expression_factor as_expression_operand aggr_mtd_attr
%type <string> opt_atomic components_list filter filter_term filter_factor
%type <string> filter_operand  filter_prefix_operand
%type <string> opt_filter_prefix_list filter_prefix_list filter_prefix_list_prefix
%type <string> filter_aspath filter_aspath_term filter_aspath_closure 
%type <string> filter_aspath_factor filter_aspath_no filter_aspath_range
%type <string> filter_rp_attribute
%type <string> opt_inject_expression inject_expression inject_expression_term
%type <string> inject_expression_factor inject_expression_operand route_inject
%type <string> router_expression opt_router_expression_with_at router_expression_term
%type <string> router_expression_factor router_expression_operand 
%type <string> filter_prefix opt_router_expression

%type <string> generic_non_empty_list generic_list list_item v4orv6addr
%type <string> action single_action opt_action tunnel opt_tunnel encapsulation
%type <string> peering import_peering_action_list export_peering_action_list
%type <string> import_simple export_simple
%type <string> import_factor export_factor 
%type <string> import_term export_term
%type <string> afi_import_exp afi_export_exp
%type <string> import_exp export_exp
%type <string> import_factor_list export_factor_list
%type <string> attr_import_syntax  attr_export_syntax
%type <string> opt_protocol_from opt_protocol_into
%type <string> opt_default_filter default_to optional_comment
%type <string> opt_afi_specification afi_list afi_token afi_name
%type <string> members_rs members_by_ref rs_member members_unit
%type <string> members_as as_member key_certif hdr_fields
%type <string> members_rtr rtr_member url auth_type auth_type_list opt_port
%type <string> opt_dirpath time_interval nserver_list optional_port
%type <string> inetnum_range domain_list

/* rs-in and rs-out attr's */

%type <string> ri_line ri_elem import_list irr_order
%type <string> export_list ri_unit irrorder_list ri_list

%token <string> T_OP_MS T_OP_TILDA T_REGEX_RANGE

/* generic's */
%type <string> repeater

/* dictionary */
%token <string> T_3DOTS T_UNION T_LIST T_OF T_OPERATOR

%type <ph> enum_list typedef_type_list typedef_type method methods
%type <ph> proto_options proto_option

/*--------Bison tokens--------*/
%token  T_CH_KEY T_DE_KEY T_AC_KEY T_TC_KEY T_MB_KEY T_NY_KEY T_OR_KEY
%token  T_RM_KEY T_SO_KEY T_PH_KEY T_FX_KEY T_EM_KEY T_NH_KEY T_DL_KEY
%token  T_IT_KEY T_ML_KEY T_MR_KEY T_RL_KEY T_NR_KEY T_RB_KEY T_AY_KEY
%token  T_RE_KEY T_QA_KEY T_RU_KEY T_SA_KEY T_SU_KEY T_RC_KEY T_EP_KEY
%token  T_HI_KEY T_DN_KEY T_ZC_KEY T_NS_KEY T_SD_KEY T_DI_KEY T_RF_KEY
%token  T_IN_KEY T_NA_KEY T_CY_KEY T_RZ_KEY T_ST_KEY T_I6_KEY T_KC_KEY
%token  T_MH_KEY T_OW_KEY T_FP_KEY T_CE_KEY T_DC_KEY T_TD_KEY T_RP_KEY
%token  T_PL_KEY T_AF_KEY T_RT_KEY T_R6_KEY T_HO_KEY T_IJ_KEY T_MO_KEY
%token  T_CO_KEY T_AB_KEY T_AG_KEY T_EC_KEY T_CU_KEY T_GX_KEY T_AN_KEY
%token  T_AA_KEY T_IP_KEY T_MI_KEY T_IV_KEY T_EX_KEY T_MX_KEY T_EV_KEY
%token  T_DF_KEY T_MD_KEY T_MA_KEY T_AK_KEY T_IR_KEY T_AZ_KEY T_LA_KEY
%token  T_IF_KEY T_PE_KEY T_MZ_KEY T_PM_KEY T_IE_KEY T_RI_KEY T_RX_KEY
%token  T_MY_KEY T_AS_KEY T_MS_KEY T_RS_KEY T_ME_KEY T_MM_KEY T_IS_KEY
%token  T_MG_KEY T_MJ_KEY T_PS_KEY T_PG_KEY T_MP_KEY T_FS_KEY T_FI_KEY
%token  T_MF_KEY T_PN_KEY T_AD_KEY T_RO_KEY T_TB_KEY T_MT_KEY T_DT_KEY
%token  T_MN_KEY T_AT_KEY T_S6_KEY T_LO_KEY T_PR_KEY T_AP_KEY T_TU_KEY
%token  T_CT_KEY T_UL_KEY T_LI_KEY T_TE_KEY T_AU_KEY T_UD_KEY T_UO_KEY
%token  T_UP_KEY T_UC_KEY

/* RPSL reserved words */

%token <string> T_ANY T_AS_ANY T_RS_ANY T_PEERAS T_AND T_OR T_NOT
%token <string> T_ATOMIC T_FROM T_TO T_AT T_ACTION T_ACCEPT T_ANNOUNCE
%token <string> T_EXCEPT T_REFINE T_NETWORKS T_INTO T_INBOUND T_OUTBOUND
%token <string> T_UPON T_HAVE_COMPONENTS T_EXCLUDE T_STATIC T_PROTOCOL
%token <string> T_MASKLEN T_RI_IMPORT T_RX_EXPORT T_IRRORDER T_AFI

/* RPSL classes */

%type <string> line_autnum   
%type <string> line_route
%type <string> line_inet_rtr
%type <string> line_dictionary
%type <string> line_route_set
%type <string> line_as_set
%type <string> line_rtr_set
%type <string> line_peering_set
%type <string> line_filter_set
%type <string> line_person
%type <string> line_role
%type <string> line_mntner
%type <string> line_key_cert
%type <string> line_repository
%type <string> line_domain
%type <string> line_inetnum

/* generic attributes */

%type <string> attr_generic 
%type <string> attr_descr
%type <string> attr_admin_c
%type <string> attr_tech_c
%type <string> attr_notify
%type <string> attr_remark
%type <string> attr_mnt_by
%type <string> attr_changed
%type <string> attr_source

%type <string> attr_mbrs_by_ref /* Note: appears in route-set, as-set
				   and rtr-set classes only */
%type <string> attr_address 
%type <string> attr_phone
%type <string> attr_fax_no
%type <string> attr_email
%type <string> attr_nic_hdl /* Note: address, phone, fax, email and nic-hdl
			       appear in person and role objects only */
/* route/route6 class */

%type <string> attr_route
%type <string> attr_route6
%type <string> attr_origin 
%type <string> attr_holes
%type <string> attr_inject
%type <string> attr_components
%type <string> attr_aggr_bndry
%type <string> attr_member_of
%type <string> attr_aggr_mtd
%type <string> attr_export_comps
%type <string> attr_roa_uri
%type <string> attr_geoidx

/* aut-num class */

%type <string> attr_autnum
%type <string> attr_import
%type <string> attr_mp_import
%type <string> attr_import_via
%type <string> attr_export
%type <string> attr_mp_export
%type <string> attr_export_via
%type <string> attr_default
%type <string> attr_mp_default
%type <string> attr_member_of_an
%type <string> attr_asname

/* inet-rtr class */

%type <string> attr_inet_rtr
%type <string> attr_alias
%type <string> attr_local_as
%type <string> attr_member_of_ir
%type <string> attr_ifaddr
%type <string> attr_interface
%type <string> attr_peer
%type <string> attr_mp_peer
%type <string> attr_rs_in
%type <string> attr_rs_out

/* dictionary class */

%type <string> attr_dictionary
%type <string> attr_rp
%type <string> attr_typedef
%type <string> attr_proto
%type <string> attr_afi

/* route-set class */

%type <string> attr_route_set
%type <string> attr_members_rs
%type <string> attr_mp_members_rs

/* as-set class */

%type <string> attr_as_set
%type <string> attr_members_as

/* rtr-set class */

%type <string> attr_rtr_set
%type <string> attr_members_rtr
%type <string> attr_mp_members_rtr

/* peering-set class */

%type <string> attr_peering_set
%type <string> attr_peering
%type <string> attr_mp_peering

/* filter-set class */

%type <string> attr_filter_set
%type <string> attr_filter
%type <string> attr_mp_filter

/* person class */

%type <string> attr_person

/* role class */

%type <string> attr_role
%type <string> attr_trouble

/* mntner class */

%type <string> attr_mntner
%type <string> attr_upd_to
%type <string> attr_mnt_nfy
%type <string> attr_auth

/* key-cert class */

%type <string> attr_key_cert
%type <string> attr_certif
%type <string> attr_method
%type <string> attr_owner
%type <string> attr_fingerpr

/* repository class */

%type <string> attr_repository
%type <string> attr_query_address
%type <string> attr_response_auth_type
%type <string> attr_submit_address
%type <string> attr_submit_auth_type
%type <string> attr_repository_cert
%type <string> attr_expire
%type <string> attr_heartbeat_interval

/* domain class */

%type <string> attr_domain
%type <string> attr_zone_c
%type <string> attr_nserver
%type <string> attr_sub_dom
%type <string> attr_dom_net
%type <string> attr_refer

/* inetnum/inet6num class */

%type <string> attr_inetnum
%type <string> attr_inet6num
%type <string> attr_netname
%type <string> attr_country
%type <string> attr_rev_srv
%type <string> attr_status

%type <string> line

%% /* grammer rules and actions follow */

input:	/* empty */
  | input line optional_comment T_NEWLINE
    { 
      if (curr_obj.attr_error & ~EMPTY_LINE)
	curr_obj.attrs[curr_obj.curr_attr]++;
      if ($2 != NULL) {
        if ($3 != NULL)	/* include comment field from attribute */
          parse_buf_add (&canon_obj, "%s  %s\n%z", $2, $3);
	else
          parse_buf_add (&canon_obj, "%s\n%z", $2);
        if (INFO_HEADERS_FLAG &&
	    (curr_obj.type == NO_OBJECT ||
	     legal_attrs[curr_obj.type][curr_obj.curr_attr]))
          build_header_info (&curr_obj, $2);
        free ($2); 
      } else
         parse_buf_add (&canon_obj, "\n%z");
      if ($3 != NULL)
	free ($3);
      if (curr_obj.attr_error & SYNTAX_ERROR)
	add_canonical_error_line (&curr_obj, &canon_obj, 0);
      start_new_attr ();
    }
  | input spec_attr
    { start_new_attr (); }
  | input error T_NEWLINE  
    { yyclearin;
      yyerrok;
      add_canonical_error_line (&curr_obj, &canon_obj, 0); 
      start_new_attr (); 
    } 
  | input error T_EOF  
    { yyclearin;
      yyerrok;
      add_canonical_error_line (&curr_obj, &canon_obj, 1);
      start_new_attr (); 
      wrap_up (&canon_obj);
    }
  | input T_NEWLINE
    { check_object_end (&curr_obj, &canon_obj); }
  | input T_EOF 		  
    { check_object_end (&curr_obj, &canon_obj);
      wrap_up (&canon_obj); 
      if (canon_obj.io == CANON_DISK) {
        fclose (canon_obj.fd);
	unlink (canon_obj.flushfn);
      }
      if (QUICK_CHECK_FLAG)
	fprintf (ofile, "Syntax OK.  Checked %d object%s\n", 
                 canon_obj.num_objs, (canon_obj.num_objs == 1 ? "." : "s."));
       return 1;
     }
     ;

spec_attr: T_UD_KEY free_text T_NEWLINE
    { if ((i = delete_syntax ($2, &curr_obj)) == 0)
        add_canonical_error_line (&curr_obj, &canon_obj, 0);
      else /* warning or good */
	parse_buf_add (&canon_obj, "%s\n%z", $2);
        free ($2);
    }
  | T_UO_KEY T_BLOB T_NEWLINE
    { curr_obj.override = strdup ($2);
      parse_buf_add (&canon_obj, "%s\n%z", $2);
      free ($2); 
    }
  | T_UO_KEY T_BLOB T_BLOB T_NEWLINE
    { curr_obj.override = strdup ($3);
      parse_buf_add (&canon_obj, "%s\n%z", $3);
      free ($2); free ($3);
    }
  | T_COOKIE free_text T_NEWLINE 
    { save_cookie_info (&curr_obj, $2); 
      curr_obj.attr_error = DEL_LINE;
      free ($2);
    }
  | T_UP_KEY free_text T_NEWLINE /* user cleartxt password for CRYPT-PW */
    { password_syntax ($2, &curr_obj);
      curr_obj.attr_error = DEL_LINE;
      free ($2);
    }
    ;

optional_comment: /* nothing */ 
      { $$ = NULL; }
    | T_COMMENT
      { $$ = $1; };

/* Classes */

line: attr_generic       { $$ = $1; }
  | line_autnum          { $$ = $1; }
  | line_route           { $$ = $1; }
  | line_inet_rtr        { $$ = $1; }
  | line_dictionary      { $$ = $1; }
  | line_route_set       { $$ = $1; }
  | line_as_set          { $$ = $1; }
  | line_rtr_set         { $$ = $1; }
  | line_peering_set     { $$ = $1; }
  | line_filter_set      { $$ = $1; }
  | line_person          { $$ = $1; }
  | line_role            { $$ = $1; }
  | line_mntner          { $$ = $1; }
  | line_key_cert        { $$ = $1; }
  | line_repository      { $$ = $1; }
  | line_domain          { $$ = $1; }
  | line_inetnum         { $$ = $1; }
  ;

/* Generic attributes */

attr_generic: attr_descr { $$ = $1; }
  | attr_admin_c         { $$ = $1; }
  | attr_tech_c          { $$ = $1; }
  | attr_notify          { $$ = $1; }
  | attr_remark          { $$ = $1; }
  | attr_mnt_by          { $$ = $1; }
  | attr_changed         { $$ = $1; }
  | attr_source          { $$ = $1; }
  | attr_mbrs_by_ref     { $$ = $1; }
  | attr_address         { $$ = $1; }
  | attr_nic_hdl         { $$ = $1; }
  | attr_phone           { $$ = $1; }
  | attr_fax_no          { $$ = $1; }
  | attr_email           { $$ = $1; }
  ;

/* repository class */

line_repository: attr_repository { $$ = $1; }
  | attr_query_address           { $$ = $1; }
  | attr_response_auth_type      { $$ = $1; } 
  | attr_submit_address          { $$ = $1; }
  | attr_submit_auth_type        { $$ = $1; }
  | attr_repository_cert         { $$ = $1; }
  | attr_expire                  { $$ = $1; }
  | attr_heartbeat_interval      { $$ = $1; }
  ;

/* key-cert class */

line_key_cert: attr_key_cert { $$ = $1; }
  | attr_certif              { $$ = $1; }
  | attr_method              { $$ = $1; }
  | attr_owner               { $$ = $1; }
  | attr_fingerpr            { $$ = $1; }
  ;

/* mntner class */

line_mntner: attr_mntner { $$ = $1; }
  | attr_upd_to          { $$ = $1; }
  | attr_mnt_nfy         { $$ = $1; }
  | attr_auth            { $$ = $1; }
  ;

/* person class */

line_person: attr_person { $$ = $1; };

/* role class */

line_role: attr_role     { $$ = $1; }
  | attr_trouble         { $$ = $1; }
  ;

/* route-set class */

line_route_set: attr_route_set { $$ = $1; }
  | attr_members_rs          { $$ = $1; }
  | attr_mp_members_rs       { $$ = $1; }
  ;

/* peering-set class */

line_peering_set: attr_peering_set { $$ = $1; }
  | attr_peering             { $$ = $1; }
  | attr_mp_peering          { $$ = $1; }
  ;

/* filter-set class */

line_filter_set: attr_filter_set { $$ = $1; }
  | attr_filter              { $$ = $1; }
  | attr_mp_filter           { $$ = $1; }
  ;

/* as-set class */

line_as_set: attr_as_set { $$ = $1; }
  | attr_members_as      { $$ = $1; }
  ;

/* rtr-set class */

line_rtr_set: attr_rtr_set { $$ = $1; }
  | attr_members_rtr       { $$ = $1; }
  | attr_mp_members_rtr    { $$ = $1; }
  ;

/* aut-num class */

line_autnum: attr_autnum { $$ = $1; }
  | attr_import          { $$ = $1; }
  | attr_mp_import       { $$ = $1; }
  | attr_import_via      { $$ = $1; }
  | attr_export          { $$ = $1; }
  | attr_mp_export       { $$ = $1; }
  | attr_export_via      { $$ = $1; }
  | attr_default         { $$ = $1; }
  | attr_mp_default      { $$ = $1; }
  | attr_member_of_an    { $$ = $1; }
  | attr_asname          { $$ = $1; }
  ;

/* route/route6 class */

line_route: attr_route { $$ = $1; }
  | attr_route6        { $$ = $1; }
  | attr_origin        { $$ = $1; }
  | attr_holes         { $$ = $1; }
  | attr_inject        { $$ = $1; }
  | attr_components    { $$ = $1; }
  | attr_export_comps  { $$ = $1; }
  | attr_aggr_bndry    { $$ = $1; }
  | attr_aggr_mtd      { $$ = $1; }
  | attr_member_of     { $$ = $1; }
  | attr_roa_uri       { $$ = $1; }
  | attr_geoidx        { $$ = $1; }
  ;

/* inet-rtr class */

line_inet_rtr: attr_inet_rtr { $$ = $1; }
  | attr_alias               { $$ = $1; }
  | attr_local_as            { $$ = $1; }
  | attr_member_of_ir        { $$ = $1; }
  | attr_ifaddr              { $$ = $1; }
  | attr_interface           { $$ = $1; }
  | attr_peer                { $$ = $1; }
  | attr_mp_peer             { $$ = $1; }
  | attr_rs_in               { $$ = $1; }
  | attr_rs_out              { $$ = $1; }
  ;

/* Dictionary class */

line_dictionary: attr_dictionary { $$ = $1; }
  | attr_rp                      { $$ = $1; }
  | attr_typedef                 { $$ = $1; }
  | attr_proto                   { $$ = $1; }
  | attr_afi                     { $$ = $1; }
  ;

/* domain class */

line_domain: attr_domain { $$ = $1; }
  | attr_zone_c          { $$ = $1; }
  | attr_nserver         { $$ = $1; }
  | attr_sub_dom         { $$ = $1; }
  | attr_dom_net         { $$ = $1; }
  | attr_refer           { $$ = $1; }
  ;

/* inetnum/inet6num class */

line_inetnum: attr_inetnum { $$ = $1; }
  | attr_inet6num      { $$ = $1; }
  | attr_netname       { $$ = $1; }
  | attr_country       { $$ = $1; }
  | attr_rev_srv       { $$ = $1; }
  | attr_status        { $$ = $1; }
  ;

/* Generic attributes */

attr_descr: T_DE_KEY non_null_lines
    { $$ = $2; }
  | T_DE_KEY
    { $$ = NULL; };
attr_admin_c: T_AC_KEY repeater
  { hdl_syntax (&curr_obj, $2); $$ = $2; };
attr_tech_c: T_TC_KEY repeater
  { hdl_syntax (&curr_obj, $2); $$ = $2; };
attr_notify: T_NY_KEY email_list { $$ = $2; };
attr_remark: T_RM_KEY non_null_lines { $$ = $2; }
  | T_RM_KEY { $$ = NULL; } ;
attr_mnt_by: T_MB_KEY mb_list { $$ = $2; };
attr_changed: T_CH_KEY T_EMAIL T_NUM
    { if ((p = date_syntax ($3, &curr_obj, 0)) == NULL) /* see if date is in future */
        p = $3;
      $$ = my_strcat (&curr_obj, 3, 02, $2, " ", p);
    }
  | T_CH_KEY T_EMAIL
    { $$ =  my_strcat (&curr_obj, 3, 02, $2, " ", todays_strdate ()); }
  ;
attr_source: T_SO_KEY T_WORD { source_syntax ($2, &curr_obj); $$ = $2; };
attr_mbrs_by_ref: T_MY_KEY members_by_ref { $$ = $2; };
attr_address: T_AD_KEY non_null_lines { $$ = $2; };
attr_nic_hdl: T_NH_KEY repeater { hdl_syntax (&curr_obj, $2); $$ = $2; };
attr_phone: T_PH_KEY non_null_lines { $$ = $2; };
attr_fax_no: T_FX_KEY non_null_lines { $$ = $2; };
attr_email: T_EM_KEY T_EMAIL { $$ = $2; };

repeater: T_BLOB          { $$ = $1; }
	| repeater T_BLOB { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

free_text:         { $$ = NULL; }
  | non_null_lines { $$ = $1; };

non_null_lines: T_NON_NULL_LINE  { $$ = $1; }
  | non_null_lines T_NON_NULL_LINE { $$ = my_strcat (&curr_obj, 3, 02, $1, "\n  ", $2); };

inetnum_range: T_SPREFIX { $$ = $1; }
  | T_SPREFIX '-' T_SPREFIX
     { inetnum_syntax (&curr_obj, $1, $3);
       $$ = my_strcat (&curr_obj, 3, 02, $1, " - ", $3); }
  | T_SPREFIX '>' T_SPREFIX
     { inetnum_syntax (&curr_obj, $1, $3);
       $$ = my_strcat (&curr_obj, 3, 02, $1, " > ", $3); };

domain_list: T_DNAME { $$ = $1; }
  | domain_list ',' T_DNAME
    { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); }
  | domain_list T_DNAME
    { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

prefix_types: T_PREFIX { $$ = $1; 
      if (curr_obj.type == O_R6)
        error_msg_queue (&curr_obj, "IPv4 prefixes not allowed in holes attribute of route6 class", ERROR_MSG); }
  | T_PREFIX6 { $$ = $1;
      if (curr_obj.type == O_RT)
        error_msg_queue (&curr_obj, "IPv6 prefixes not allowed in holes attribute of route class", ERROR_MSG); };

prefix_list: prefix_types { $$ = $1; }
  | prefix_list ',' prefix_types
    { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

mb_list: T_WORD { $$ = $1; mb_check (&curr_obj, $1); }
  | mb_list ',' T_WORD
    { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3);
    mb_check (&curr_obj, $3); };

email_list: T_EMAIL { $$ = $1; }
  | email_list T_EMAIL { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

/* domain class */

attr_domain: T_DN_KEY T_DNAME { $$ = $2; };
attr_zone_c: T_ZC_KEY repeater { hdl_syntax (&curr_obj, $2); $$ = $2; };
attr_nserver: T_NS_KEY nserver_list { $$ = $2; };
attr_sub_dom: T_SD_KEY domain_list  { $$ = $2; };
attr_dom_net: T_DI_KEY sprefix_list { $$ = $2; };
attr_refer:   T_RF_KEY T_REFER_KEYWORD T_DNAME optional_port
  { $$ = my_strcat (&curr_obj, 5, 02|010, $2, " ", $3, " ", $4); };

optional_port: /* nothing */ 
      { $$ = NULL; }
    | T_NUM { $$ = $1; };
   
nserver_list: host_name { $$ = $1; }
  | nserver_list ',' host_name
    { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); }
  | nserver_list host_name
    { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

sprefix_list: T_SPREFIX { $$ = $1; }
  | sprefix_list ',' T_SPREFIX
    { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); }
  | sprefix_list T_SPREFIX
    { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

/* inetnum/inet6num common */
attr_netname: T_NA_KEY T_WORD { $$ = $2; };
attr_country: T_CY_KEY T_WORD { country_syntax ($2, &curr_obj); $$ = $2; };
attr_rev_srv: T_RZ_KEY T_DNAME { $$ = $2; };
attr_status: T_ST_KEY non_null_lines { $$ = $2; }
  | T_ST_KEY { $$ = NULL; } ;

/* inetnum class */
attr_inetnum: T_IN_KEY inetnum_range { $$ = $2; };

/* inet6num class */
attr_inet6num: T_I6_KEY T_PREFIX6 { $$ = $2; };

/* repository class */

attr_repository: T_RE_KEY T_WORD { $$ = $2; };

attr_query_address:  T_QA_KEY url { $$ = $2; };
attr_submit_address: T_SA_KEY url { $$ = $2; };

url: T_RPSQUERY host_name opt_port 
      { $$ = my_strcat (&curr_obj, 3, 0, $1, $2, $3); }
    | T_WHOIS host_name opt_port
      { $$ = my_strcat (&curr_obj, 3, 0, $1, $2, $3); }
    | T_MAILTO T_EMAIL
      { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); }
    | T_FTP host_name opt_dirpath opt_port
      { $$ = my_strcat (&curr_obj, 4, 0, $1, $2, $3, $4); };

opt_dirpath: /* nothing */
      { $$ = NULL; }
    | T_DIRPATH { $$ = $1; };

opt_port: /* nothing */ 
      { $$ = NULL; }
    | ':' T_NUM { $$ = my_strcat (&curr_obj, 2, 01, ":", $2); };
   
attr_response_auth_type: T_RU_KEY auth_type_list { $$ = $2; };
attr_submit_auth_type:   T_SU_KEY auth_type_list { $$ = $2; };

auth_type_list: auth_type { $$ = $1; }
  | auth_type_list ','  auth_type
    { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

auth_type: T_PGPKEY { $$ = $1; }
  | T_MAILFROM      { $$ = $1; }
  | T_CRYPTPW       { $$ = $1; }
  | T_MD5PW         { $$ = $1; }
  | T_NONE          { $$ = $1; };

attr_repository_cert: T_RC_KEY T_KCHEXID { $$ = $2; };

attr_expire: T_EX_KEY time_interval { $$ = $2; };
attr_heartbeat_interval: T_HI_KEY time_interval { $$ = $2; };

time_interval: T_NUM T_NUM ':' T_NUM ':' T_NUM
     { if ((p = time_interval_syntax (&curr_obj, $1, $2, $4, $6)) != NULL)
         $$ = p;
       else
         $$ = my_strcat (&curr_obj, 7, 02|010|040, $1, " ", $2, ":", $4, ":", $6); 
     };

/* key-cert class */

attr_key_cert: T_KC_KEY T_KCHEXID
    { $$ = $2; strcpy (curr_obj.u.kc.kchexid, (char *) ($2 + 7)); };

attr_method: T_MH_KEY machine_generated { $$ = NULL; };

attr_owner: T_OW_KEY machine_generated { $$ = NULL; };

attr_fingerpr: T_FP_KEY machine_generated { $$ = NULL; };

machine_generated: free_text
   { free ($1); $$ = NULL; curr_obj.attr_error = MACHINE_GEN_LINE; };

attr_certif: T_CE_KEY T_BEGIN_PGP_BLOCK hdr_fields key_certif T_END_PGP_BLOCK
   { curr_obj.u.kc.certif = 
       my_strcat (&curr_obj, 5, 01|02|010|020, $2, "\n", 
		  curr_obj.u.kc.certif, $5, "\n");
     $$ = my_strcat (&curr_obj, 8, 01|04|020|0100, "\n ", $2, "\n ", $3, 
		     "\n+\n", $4, " ", $5);
     curr_obj.keycertfn = hexid_check (&curr_obj); };

hdr_fields: T_VERSION 
    { curr_obj.u.kc.certif = my_strcat (&curr_obj, 2, 01|02, $1, "\n");
      $$ = $1;
    }
  | hdr_fields T_VERSION
    { curr_obj.u.kc.certif = 
	my_strcat (&curr_obj, 3, 02|04, curr_obj.u.kc.certif, $2, "\n"); 
      $$ = my_strcat (&curr_obj, 3, 02, $1, "\n ", $2);
    };

key_certif: T_RADIX64
     { curr_obj.u.kc.certif = my_strcat (&curr_obj, 4, 02|04|010, 
					 curr_obj.u.kc.certif, "\n", $1, "\n");
       $$ = my_strcat (&curr_obj, 3, 01|04, " ", $1, "\n"); 
     }
   | key_certif T_RADIX64
     { curr_obj.u.kc.certif = 
	 my_strcat (&curr_obj, 3, 02|04, curr_obj.u.kc.certif, $2, "\n");
       $$ = my_strcat (&curr_obj, 4, 02|010, $1, " ", $2, "\n"); 
     };

/* mntner class */

attr_mntner: T_MT_KEY T_WORD { $$ = $2; mb_check (&curr_obj, $2); };
attr_upd_to: T_DT_KEY T_EMAIL { $$ = $2; };
attr_mnt_nfy: T_MN_KEY T_EMAIL { $$ = $2; };
attr_auth: T_AT_KEY auth_line { $$ = $2; };

auth_line: T_KCHEXID { $$ = $1; }
  | T_MAILFROM T_BLOB
    { regex_syntax ($2, &curr_obj); 
      $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); }
  | T_CRYPTPW T_BLOB
    { cryptpw_syntax ($2, &curr_obj); 
      $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); }
  | T_MD5PW T_BLOB
    { md5pw_syntax ($2, &curr_obj); 
      $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); }
  | T_NONE { $$ = $1; };

/* person class */

attr_person: T_PN_KEY non_null_lines { hdl_syntax (&curr_obj, $2); $$ = $2; };

/* role class */

attr_role: T_RO_KEY non_null_lines { $$ = $2; };
attr_trouble: T_TB_KEY non_null_lines { $$ = $2; };

/* peering-set class */

attr_peering_set: T_PS_KEY T_PRNGNAME { $$ = $2; };
attr_peering: T_PG_KEY {mp_attr = 0;} peering { $$ = $3; };
attr_mp_peering: T_MP_KEY {mp_attr = 1;} peering { $$ = $3; };

/* filter-set class */

attr_filter_set: T_FS_KEY T_FLTRNAME { $$ = $2; };
attr_filter: T_FI_KEY {mp_attr = 0;} filter { $$ = $3; };
attr_mp_filter: T_MF_KEY {mp_attr = 1;} filter { $$ = $3; };

/* route-set class */

attr_route_set: T_RS_KEY T_RSNAME
    { $$ = $2; if (xx_set_syntax ("rs-any", $2))
        error_msg_queue (&curr_obj, "Reserved word 'rs-any' not allowed", ERROR_MSG); };

attr_members_rs: T_ME_KEY {mp_attr = 0;} members_rs { $$ = $3; };
attr_mp_members_rs: T_MM_KEY {mp_attr = 1;} members_rs { $$ = $3; };

members_rs: rs_member { $$ = $1; }
  | members_rs ',' rs_member 
    { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

rs_member: T_AS      { $$ = $1; }
  | T_AS T_OP_MS     { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); }
  | T_ASNAME         { $$ = $1; }
  | T_ASNAME T_OP_MS { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); }
  | T_RSNAME         { $$ = $1; }
  | T_RSNAME T_OP_MS { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); }
  | T_PREFIX         { $$ = $1; } 
  | T_PREFIXRNG      { $$ = $1; }
  | T_PREFIX6        { if (mp_attr == 0) {
       error_msg_queue (&curr_obj, "Use of IPv6 prefix(es) is not allowed in members attribute, please use mp-members instead", ERROR_MSG); }
       $$ = $1; }
  | T_PREFIXRNG6     { if (mp_attr == 0) {
       error_msg_queue (&curr_obj, "Use of IPv6 prefix(es) is not allowed in members attribute, please use mp-members instead", ERROR_MSG); }
       $$ = $1; };

members_unit: T_WORD { $$ = $1;}
  | T_ANY { $$ = $1; };

members_by_ref: members_unit { $$ = $1; }
  | members_by_ref ',' members_unit { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

/* as-set class */

attr_as_set: T_AS_KEY T_ASNAME
    { $$ = $2;
      if (xx_set_syntax ("as-any", $2))
        error_msg_queue (&curr_obj, "Reserved word 'as-any' not allowed", ERROR_MSG); };
attr_members_as: T_MS_KEY members_as { $$ = $2; };

members_as: as_member { $$ = $1; }
  | members_as ',' as_member { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

as_member: T_AS { $$ = $1; }
  | T_ASNAME { $$ = $1; };

/* rtr-set class */

attr_rtr_set: T_IS_KEY T_RTRSNAME { $$ = $2; };
attr_members_rtr: T_MG_KEY {mp_attr = 0;} members_rtr { $$ = $3; };
attr_mp_members_rtr: T_MJ_KEY {mp_attr = 1;} members_rtr { $$ = $3; };

members_rtr: rtr_member { $$ = $1; }
  | members_rtr ',' rtr_member { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

rtr_member: T_RTRSNAME { $$ = $1; }
  | T_SPREFIX     { $$ = $1; }
  | T_SPREFIX6    { if (mp_attr == 0) {
       error_msg_queue (&curr_obj, "Use of IPv6 address not allowed", ERROR_MSG); }
       $$ = $1; }
  | T_DNAME       { $$ = $1; };

/* inet-rtr class */

attr_inet_rtr: T_IR_KEY host_name { $$ = $2; };

attr_alias: T_AZ_KEY domain_list { $$ = $2; };

attr_local_as: T_LA_KEY T_AS { $$ = $2; };

attr_member_of_ir: T_MZ_KEY ir_member_of { $$ = $2; };

attr_rs_in: T_RI_KEY { ri_opts = all = none = 0;} ri_line { $$ = $3; };

attr_rs_out: T_RX_KEY { ri_opts = all = none = 0;} export_list { $$ = $3; };

attr_interface: T_IE_KEY {mp_attr = 1;} v4orv6addr T_MASKLEN T_NUM opt_action opt_tunnel
  { $$ = my_strcat (&curr_obj, 9, 02|010|040, $3, " ", $4, " ", $5, " ", $6, " ", $7); };

attr_ifaddr: T_IF_KEY T_SPREFIX T_MASKLEN T_NUM opt_action
  { $$ = my_strcat (&curr_obj, 7, 02|010|040, $2, " ", $3, " ", $4, " ", $5); };

attr_peer: T_PE_KEY T_WORD 
  { if ((pr = find_protocol (&proto_ll, $2)) == NULL) {
      snprintf (err_buf, ERR_BUF_SIZE, "Unrecognized protocol \"%s\"", $2);
      error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
    }
  }
  peer_id opt_peer_options
  { $$ = my_strcat (&curr_obj, 5, 02|010, $2, " ", $4, " ", $5); };

attr_mp_peer: T_PM_KEY {mp_attr = 1;} T_WORD 
  { if ((pr = find_protocol (&proto_ll, $3)) == NULL) {
      snprintf (err_buf, ERR_BUF_SIZE, "Unrecognized protocol \"%s\"", $3);
      error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
    }
  }
  mp_peer_id opt_peer_options
  { $$ = my_strcat (&curr_obj, 5, 02|010, $3, " ", $5, " ", $6); };

host_name: T_SPREFIX { $$ = $1; }
  | T_DNAME         { $$ = $1; };

peer_id: T_SPREFIX { $$ = $1; }
  | T_DNAME       { $$ = $1; }
  | T_RTRSNAME    { $$ = $1; }
  | T_PRNGNAME    { $$ = $1; };

mp_peer_id:  v4orv6addr { $$ = $1; }
  | T_DNAME     { $$ = $1; }
  | T_RTRSNAME  { $$ = $1; }
  | T_PRNGNAME  { $$ = $1; } ;

opt_peer_options: { $$ = NULL; }
  | peer_options  { $$ = $1; } ;
  
peer_options: peer_option        { $$ = $1; }
  | peer_options ',' peer_option { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

peer_option: T_WORD '(' generic_list ')' 
  { if (pr != NULL) {
      if ((method = find_method (pr->first, $1)) == NULL) {
        snprintf (err_buf, ERR_BUF_SIZE,
	         "\"%s\" is an illegal protocol option for protocol \"%s\"", $1, pr->name);
        error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
      }
      else if (!valid_args (&curr_obj, method, 1, $1, 1, &$3))
	curr_obj.error_pos = tokenpos;
    }
    $$ = my_strcat (&curr_obj, 4, 02|010, $1, "(", $3, ")");  
  };


ir_member_of: T_RTRSNAME         { $$ = $1; }
   | ir_member_of ',' T_RTRSNAME { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };


/* ================================================================ */

ri_line: ri_elem { $$ = $1; }
  | ri_line ',' ri_elem { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

ri_elem: import_list { $$ = $1; /* JW take out ri_opts |= IMPORT_F; */ }
  | T_WORD
    { $$ = $1;
      if (!strcasecmp ($1, "aspath_trans")) {
	if (ri_opts & ASPATH_TRANS_F)
	  error_msg_queue (&curr_obj, "\"aspath_trans\" multiply defined", WARN_OVERRIDE_MSG);
        ri_opts |= ASPATH_TRANS_F;
      }
      else if (!strcasecmp ($1, "flap_dampen")) {
	if (ri_opts & FLAP_DAMP_F)
	  error_msg_queue (&curr_obj, "\"flap_dampen\" multiply defined", WARN_OVERRIDE_MSG);
        ri_opts |= FLAP_DAMP_F;
      }
      else {
        snprintf (err_buf, ERR_BUF_SIZE, "Unknown route server directive \"%s\"", $1);
        error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
      }
    }
  | irr_order 
    { $$ = $1;
      if (ri_opts & IRRORDER_F)
        error_msg_queue (&curr_obj, "\"irrorder\" multiply defined", ERROR_MSG);
      ri_opts |= IRRORDER_F;
    }
  ;

irr_order: T_IRRORDER '(' irrorder_list ')'
    { $$ = my_strcat (&curr_obj, 4, 02|010, $1, "(", $3, ")"); };

irrorder_list: T_WORD
    { $$ = $1; 
      import_errors = irrorder_syntax (&curr_obj, NULL, $1); 
    }
  | irrorder_list ',' T_WORD 
    { if (!import_errors)
        import_errors = irrorder_syntax (&curr_obj, $1, $3);
        $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

import_list: T_RI_IMPORT '(' ri_list ')'
    { $$ = my_strcat (&curr_obj, 4, 02|010, $1, "(", $3, ")"); };

export_list: T_RX_EXPORT '(' ri_list ')' 
    { $$ = my_strcat (&curr_obj, 4, 02|010, $1, "(", $3, ")"); };

ri_list: ri_unit { $$ = $1; }
  | ri_list ',' ri_unit { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

ri_unit: T_WORD 
    { $$ = $1;
      if (!strcasecmp ($1, "all")) {
        if (++all == 2) 
          error_msg_queue (&curr_obj, "\"ALL\" multiply defined", ERROR_MSG);
        if (none && all == 1)
          error_msg_queue (&curr_obj, "\"ALL\" and \"NONE\" are ambiguous", ERROR_MSG);
      }
      else if (!strcasecmp ($1, "none")) {
        if (++none == 2) 
          error_msg_queue (&curr_obj, "\"NONE\" multiply defined", ERROR_MSG);
        if (all && none == 1)
          error_msg_queue (&curr_obj, "\"ALL\" and \"NONE\" are ambiguous", ERROR_MSG);
      }
      else {
        snprintf (err_buf, ERR_BUF_SIZE, "Unknown list element \"%s\"", $1);
        error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
      }
    }
  | T_AS         { $$ = $1; }
  | T_ASNAME     { $$ = $1; }
  | '!' T_AS     { $$ = my_strcat (&curr_obj, 2, 01, "!", $2); }
  | '!' T_ASNAME { $$ = my_strcat (&curr_obj, 2, 01, "!", $2); }
  ; 

/* ================================================================ */

/*  aut-num class */

attr_autnum: T_AN_KEY T_AS { asnum_syntax ($2, &curr_obj); $$ = $2; };

attr_asname: T_AA_KEY T_WORD { $$ = $2; };

attr_import: T_IP_KEY {mp_attr = 0; via_attr = 0;} attr_import_syntax { $$ = $3; };

attr_mp_import: T_MI_KEY {mp_attr = 1; via_attr = 0;} attr_import_syntax { $$ = $3; };

attr_import_via: T_IV_KEY {mp_attr = 1; via_attr = 1;} attr_import_syntax { $$ = $3; };

attr_import_syntax: opt_protocol_from opt_protocol_into import_simple
           { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
   | opt_protocol_from opt_protocol_into afi_import_exp
           { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); };

attr_export: T_EX_KEY {mp_attr = 0; via_attr=0;} attr_export_syntax { $$ = $3; };

attr_mp_export: T_MX_KEY {mp_attr = 1; via_attr=0;} attr_export_syntax { $$ = $3; };

attr_export_via: T_EV_KEY {mp_attr = 1; via_attr = 1;} attr_export_syntax { $$ = $3; };

attr_export_syntax: opt_protocol_from opt_protocol_into export_simple
           { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
   | opt_protocol_from opt_protocol_into afi_export_exp
           { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); };

attr_member_of_an: T_MA_KEY an_member_of { $$ = $2; };

an_member_of: T_ASNAME         { $$ = $1; }
   | an_member_of ',' T_ASNAME { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

attr_default: T_DF_KEY {mp_attr = 0;} default_to { $$ = $3; };

attr_mp_default: T_MD_KEY {mp_attr = 1;} opt_afi_specification default_to
     { $$ = my_strcat (&curr_obj, 3, 02, $3, " ", $4); };

default_to: T_TO peering opt_action opt_default_filter
     { $$ = my_strcat (&curr_obj, 7, 02|010|040, $1, " ", $2, " ", $3, " ", $4); };

opt_default_filter:    { $$ = NULL; }
   | T_NETWORKS filter { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

/* afi */

opt_afi_specification: { $$ = NULL; }
  | T_AFI  { if (mp_attr == 0) {
       error_msg_queue (&curr_obj, "Use of AFI type in non-multiprotocol attribute", ERROR_MSG); }
     } afi_list { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $3);};

afi_list: afi_token { $$ = $1; }
  | afi_list ',' afi_token
      { $$ = my_strcat (&curr_obj, 3, 02, $1, ",", $3); };

afi_token: afi_name { if (find_afi (&afi_ll, $1) == NULL) {
       snprintf (err_buf, ERR_BUF_SIZE, "Unrecognized afi \"%s\"", $1);
       error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
       }
       $$ = $1;
     };

/* protocol */

opt_protocol_from: { $$ = NULL; }
        | T_PROTOCOL T_WORD
            { if (find_protocol (&proto_ll, $2) == NULL) {
                   snprintf (err_buf, ERR_BUF_SIZE, "Unrecognized protocol \"%s\"", $2);
                   error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
	      }
              $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

opt_protocol_into: { $$ = NULL; }
        | T_INTO T_WORD
            { if (find_protocol (&proto_ll, $2) == NULL) {
                   snprintf (err_buf, ERR_BUF_SIZE, "Unrecognized protocol \"%s\"", $2);
                   error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
	      }
              $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); }; 

/* simple import/export expression */

import_simple: opt_afi_specification import_factor
        { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

export_simple: opt_afi_specification export_factor
        { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

/* structured import/export expressions */

afi_import_exp: opt_afi_specification import_exp
        { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

afi_export_exp: opt_afi_specification export_exp
        { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

import_exp: import_term
          { $$ =  $1; }
        | import_term T_EXCEPT afi_import_exp
          { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | import_term T_REFINE afi_import_exp
          { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); };

import_factor_list: import_factor ';'
          { $$ = my_strcat (&curr_obj, 2, 02, $1, ";"); }
        | import_factor_list import_factor ';'
          { $$ = my_strcat (&curr_obj, 4, 02|010, $1, " ", $2, ";"); };

import_term: import_factor ';'
          { $$ = my_strcat (&curr_obj, 2, 02, $1, ";"); }
        | '{' import_factor_list '}'
          { $$ = my_strcat (&curr_obj, 3, 01|04, "{", $2, "}"); };

export_exp: export_term
          { $$ =  $1; }
        | export_term T_EXCEPT afi_export_exp
          { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | export_term T_REFINE afi_export_exp
          { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); };

export_factor_list: export_factor ';'
          { $$ = my_strcat (&curr_obj, 2, 02, $1, ";"); }
        | export_factor_list export_factor ';'
          { $$ = my_strcat (&curr_obj, 4, 02|010, $1, " ", $2, ";"); };

export_term: export_factor ';'
          { $$ = my_strcat (&curr_obj, 2, 02, $1, ";"); }
        | '{' export_factor_list '}'
          { $$ = my_strcat (&curr_obj, 3, 01|04, "{", $2, "}"); };

/* import/export factor */

import_factor: import_peering_action_list T_ACCEPT filter
        { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); };

export_factor: export_peering_action_list T_ANNOUNCE filter
        { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); };

/* peering action pair */

import_peering_action_list: T_FROM peering opt_action
            { if ( via_attr == 1 )
                error_msg_queue (&curr_obj, "illegal syntax clause i-1", ERROR_MSG); 
              $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | peering T_FROM peering opt_action 
            { if ( via_attr == 0 )
                error_msg_queue (&curr_obj, "illegal syntax clause i-2", ERROR_MSG); 
              $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | import_peering_action_list T_FROM peering opt_action 
            { if ( via_attr == 1 )
                error_msg_queue (&curr_obj, "illegal syntax clause i-3", ERROR_MSG); 
              $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | import_peering_action_list peering T_FROM peering opt_action 
            { if ( via_attr == 0 )
                error_msg_queue (&curr_obj, "illegal syntax clause i-4", ERROR_MSG); 
              $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); };

export_peering_action_list: T_TO peering opt_action
            { if ( via_attr == 1 )
                error_msg_queue (&curr_obj, "illegal syntax clause e-1", ERROR_MSG); 
              $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | peering T_TO peering opt_action
            { if ( via_attr == 0 )
                error_msg_queue (&curr_obj, "illegal syntax clause e-2", ERROR_MSG); 
              $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | export_peering_action_list T_TO peering opt_action
            { if ( via_attr == 1 )
                error_msg_queue (&curr_obj, "illegal syntax clause e-3", ERROR_MSG); 
              $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | export_peering_action_list peering T_TO peering opt_action 
            { if ( via_attr == 0 )
                error_msg_queue (&curr_obj, "illegal syntax clause e-4", ERROR_MSG); 
              $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); };

peering: as_expression opt_router_expression opt_router_expression_with_at
            { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | T_PRNGNAME { $$ = $1; };

/* Dictionary */

attr_dictionary: T_DC_KEY T_WORD { $$ = $2; };

attr_rp: T_RP_KEY T_WORD methods
     { add_rp_attr (&rp_attr_ll, create_rp_attr ($2, $3->method));
       $$ = my_strcat (&curr_obj, 3, 02, $2, " ", $3->cs);
       free ($3);
       /* JW fprintf (dfile, "rp_attr (%s)\n", $$);
	  print_rp_list (&rp_attr_ll);
	  print_typedef_list (&type_ll); */ }
   | T_RP_KEY T_RP_ATTR methods
     { add_rp_attr (&rp_attr_ll, create_rp_attr ($2, $3->method));
       $$ = my_strcat (&curr_obj, 3, 02, $2, " ", $3->cs);
       free ($3); };

methods: method
     { $$ = $1; fprintf (dfile, "methods: method (%s)\n", $$->cs); }
   | methods method
     { $$ = $1;
       $$->method = add_method ($1->method, $2->method); 
       $$->cs     = my_strcat (&curr_obj, 3, 02, $1->cs, " ", $2->cs);
       free ($2); };

method: T_WORD '(' ')'
     { create_placeholder ($$);
       $$->method = create_method ($1, NULL, LIST_TYPE, 0);
       $$->cs     = my_strcat (&curr_obj, 3, 02|04, $1, "(", ")");
     }
   | T_WORD '(' typedef_type_list ')'
     { $$ = $3;
       $$->method = create_method ($1, $3->parm, LIST_TYPE, 0);
       $$->cs     = my_strcat (&curr_obj, 4, 02|010, $1, "(", $3->cs, ")");
     }
   | T_WORD '(' typedef_type_list ',' T_3DOTS ')'
     { $$ = $3;
       $$->method = create_method ($1, $3->parm, LIST_TYPE, 1);
       $$->cs     = my_strcat (&curr_obj, 5, 02|010, $1, "(", $3->cs, ", ", $5);
     }
   | T_OPERATOR T_OP '(' typedef_type_list ')'
     { $$ = $4;
       $$->method = create_method ($2, $4->parm, find_arg_context ($4->parm, 0), 0);
       $$->cs     = my_strcat (&curr_obj, 5, 04|020, $1, $2, "(", $4->cs, ")");
     }
   | T_OPERATOR T_OP '(' typedef_type_list ',' T_3DOTS ')'
     { $$ = $4;
       $$->method = create_method ($2, $4->parm, LIST_TYPE, 1);
       $$->cs     = my_strcat (&curr_obj, 6, 04|020, $1, $2, "(", $4->cs, ", ", $6);
     };

attr_typedef: T_TD_KEY T_WORD typedef_type
     { create_type (&curr_obj, TYPEDEF, $2, $3->type);
       $$ = my_strcat (&curr_obj, 3, 02, $2, " ", $3->cs);
       free ($3);
       /*JW print_typedef_list (&type_ll); */
     };

typedef_type_list: typedef_type
     { $$ = $1;
       $$->parm = create_parm ($1->type);
     }
   | typedef_type_list ',' typedef_type
     { $$ = $1;
       $$->parm = add_parm_obj ($1->parm, create_parm ($3->type));
       $$->cs   = my_strcat (&curr_obj, 3, 02, $1->cs, ", ", $3->cs);
       free ($3);
     };

typedef_type: T_UNION typedef_type_list
      { $$ = $2;
        $$->type = create_type (&curr_obj, UNION, $2->parm);
        $$->cs   = my_strcat (&curr_obj, 3, 02, $1, " ", $2->cs);
      }
   | T_WORD
      { create_placeholder ($$);
        $$->type = create_type (&curr_obj, UNKNOWN, $1);
        $$->cs   = $1;
      }
   | T_WORD '[' T_NUM ',' T_NUM ']'
      { create_placeholder ($$);
        $$->type = create_type (&curr_obj, PREDEFINED, INTEGER, $3, $5);
        $$->cs   = my_strcat (&curr_obj, 6, 02|010|040, $1, "[", $3, ", ", $5, "]");
      }
   | T_WORD '[' T_REAL ',' T_REAL ']'
      { create_placeholder ($$);
        $$->type = create_type (&curr_obj, PREDEFINED, REAL, $3, $5);
        $$->cs   = my_strcat (&curr_obj, 6, 02|010|040, $1, "[", $3, ", ", $5, "]");
      }
   | T_WORD '[' enum_list ']'
      { create_placeholder ($$);
	$$->type = create_type (&curr_obj, PREDEFINED, ENUM, $3->en);
        $$->cs   = my_strcat (&curr_obj, 4, 02|010, $1, "[", $3->cs, "]");
      }
   | T_LIST '[' T_NUM ':' T_NUM ']' T_OF typedef_type
      { $$ = $8;
	$$->type = create_type (&curr_obj, LIST, $8->type, $3, $5);
        $$->cs   = my_strcat (&curr_obj, 9, 02|010|040|0200, $1, " [", $3, ":", $5, "] ", $7, " ", $8->cs); 
      }
   | T_LIST T_OF typedef_type
      { $$ = $3;
	$$->type = create_type (&curr_obj, LIST, $3->type, NULL);
        $$->cs   = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3->cs);
      };

enum_list: T_WORD
      { create_placeholder ($$);
        $$->en = append_enum (NULL, $1);
        $$->cs = $1; }
   | enum_list ',' T_WORD
      { $$ = $1;
        $$->en = append_enum ($1->en, $3);
        $$->cs = my_strcat (&curr_obj, 3, 02, $$->cs, ",", $3); };

afi_name: T_WORD { $$ = $1; }
  | T_DNAME      { $$ = $1; };

attr_afi: T_AF_KEY afi_name
   { add_new_afi (&afi_ll, create_afi_attr($2));
     $$ = $2;
   };

attr_proto: T_PL_KEY T_WORD proto_options 
   {  if ($3 != NULL) {
        add_new_proto (&proto_ll, create_proto_attr ($2, $3->method));
	$$ = my_strcat (&curr_obj, 3, 02, $2, " ", $3->cs);
	free ($3);
      } else {
        add_new_proto (&proto_ll, create_proto_attr ($2, NULL));
        $$ = $2;
      }
      /* print_proto_list (&proto_ll); */
   };

proto_options: { $$ = NULL; }
   | proto_options proto_option
     { if ($1 == NULL) { 
         $$ = $2;
       } else {
	 $$ = $1;
         $$->method = add_method ($1->method, $2->method); 
         $$->cs     = my_strcat (&curr_obj, 3, 02, $1->cs, " ", $2->cs);
         free ($2);
       }
     };

proto_option: T_MANDATORY method 
      { $$ = $2;
        $$->cs = my_strcat (&curr_obj, 3, 02, $1, " ", $2->cs);
      }
   | T_OPTIONAL method
      { $$ = $2;
        $$->cs = my_strcat (&curr_obj, 3, 02, $1, " ", $2->cs);
      };

/* common to both route and route6 classes */

attr_origin:       T_OR_KEY T_AS { asnum_syntax ($2, &curr_obj); $$ = $2; };
attr_aggr_bndry:   T_AB_KEY as_expression { $$ = $2; };
attr_aggr_mtd:     T_AG_KEY aggr_mtd_attr { $$ = $2; };
attr_member_of:    T_MO_KEY rt_member_of { $$ = $2; };

/* route/route6 classes */

attr_route:        T_RT_KEY T_PREFIX { $$ = $2; }; 
attr_route6:       T_R6_KEY T_PREFIX6 { $$ = $2; }; 
attr_holes:        T_HO_KEY prefix_list { $$ = $2; };
attr_inject:       T_IJ_KEY route_inject { $$ = $2; };
attr_export_comps: T_EC_KEY filter { $$ = $2; };
attr_roa_uri: T_CU_KEY non_null_lines { $$ = $2; };
attr_geoidx: T_GX_KEY non_null_lines { $$ = $2; };
attr_components:   T_CO_KEY { filter_count = 0; } opt_atomic components_list
  { if (filter_count)
      $$ = my_strcat (&curr_obj, 2, 0, $3, $4);
    else {
      $$ = NULL;
      add_canonical_error_line (&curr_obj, &canon_obj, 0);
      error_msg_queue (&curr_obj,"Empty attribute removed", EMPTY_ATTR_MSG);
    }
  };

rt_member_of: T_RSNAME         { $$ = $1; }
  | rt_member_of ',' T_RSNAME { $$ = my_strcat (&curr_obj, 3, 02, $1, ", ", $3); };

route_inject: opt_router_expression_with_at opt_action opt_inject_expression
    { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); 
      if (*$$ == '\0') {
        error_msg_queue (&curr_obj, "Empty attribute removed", EMPTY_ATTR_MSG); 
      }
    };

opt_tunnel: { $$ = NULL; }
   | T_TUNNEL tunnel
      { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

tunnel: v4orv6addr ',' encapsulation
       { $$ = my_strcat (&curr_obj, 4, 02|04, $1, "," , " ", $3); };

encapsulation: T_GRE { $$ = $1; }
  | T_IPINIP      { $$ = $1; };

v4orv6addr: T_SPREFIX { $$ = $1;
      if (curr_obj.type == O_R6)
        error_msg_queue (&curr_obj, "IPv4 prefixes not allowed in attributes of route6 class", ERROR_MSG); }
  | T_SPREFIX6 {  $$ = $1;
      if (curr_obj.type != O_R6 && (curr_obj.type == O_RT || mp_attr == 0) )
        error_msg_queue (&curr_obj, "IPv6 prefixes not permitted", ERROR_MSG); };

opt_action: { $$ = NULL; }
   | T_ACTION action
      { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

action: single_action { $$ = $1; }
   | action single_action 
      { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

single_action: T_RP_ATTR '.' T_WORD '(' generic_list ')' ';'
      { 
        fprintf (dfile, "\n\n++++++++++++\n rp_attr=(%s) word=(%s) generic_list=(%s)\n", $1, $3, $5);
        rp = is_RPattr (&rp_attr_ll, $1);
        if ( rp==NULL || (method = find_method (rp->first, $3)) == NULL) {
          snprintf (err_buf, ERR_BUF_SIZE,
		   "\"%s\" is an illegal RPSL method for RP attribute \"%s\"", $3, $1);
	  error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
	}
	else if (!valid_args (&curr_obj, method, 1, $1, 1, &$5))
	  curr_obj.error_pos = tokenpos;
	$$ = my_strcat (&curr_obj, 7, 02|010|040|0100, $1, ".", $3, "(", $5, ")", ";");
      }
   | T_RP_ATTR T_OP list_item ';'
      { 
        fprintf (dfile, "\n\n++++++++++++\n rp_attr=(%s) op=(%s) list_item=(%s)\n", $1, $2, $3);
        rp = is_RPattr (&rp_attr_ll, $1);
        if ( rp==NULL || (method = find_method (rp->first, $2)) == NULL) {
	  snprintf (err_buf, ERR_BUF_SIZE,
		   "\"%s\" is an illegal RPSL operator for RP attribute \"%s\"", $2, $1);
	  error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
	}
	else if (!valid_args (&curr_obj, method, 0, $1, 1, &$3))
	  curr_obj.error_pos = tokenpos;
	$$ = my_strcat (&curr_obj, 6, 02|010|040, $1, " ", $2, " ", $3, ";");
      }
   | T_RP_ATTR '(' generic_list ')' ';'
      { 
        fprintf (dfile, "\n\n++++++++++++\n rp_attr=(%s) generic_list=(%s)\n", $1, $3);
        rp = is_RPattr (&rp_attr_ll, $1);
        if ( rp==NULL || (method = find_method (rp->first, "()")) == NULL) {
	  snprintf (err_buf, ERR_BUF_SIZE,
		   "\"()\" is an illegal RPSL operator for RP attribute \"%s\"", $1);
	  error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
	}
	else if (!valid_args (&curr_obj, method, 1, $1, 1, &$3))
	  curr_obj.error_pos = tokenpos;
	$$ = my_strcat (&curr_obj, 5, 02|010|020, $1, "(", $3, ")", ";");
      }
   | T_RP_ATTR '[' generic_list ']' ';'
      { rp = is_RPattr (&rp_attr_ll, $1);
	if ( rp == NULL || (method = find_method (rp->first, "[]")) == NULL) {
	  snprintf (err_buf, ERR_BUF_SIZE,
		   "\"[]\" is an illegal RPSL operator for RP attribute \"%s\"", $1);
	  error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
	}
	else if (!valid_args (&curr_obj, method, 1, $1, 1, &$3))
	  curr_obj.error_pos = tokenpos;
	$$ = my_strcat (&curr_obj, 5, 02|010|020, $1, "[", $3, "]", ";");
      }
   | ';' { $$ = strdup (";"); };

generic_list:               { $$ = NULL; }
   | generic_non_empty_list { $$ = $1; };

generic_non_empty_list: list_item { $$ = $1; }
   | generic_non_empty_list ',' list_item 
      { $$ = my_strcat (&curr_obj, 4, 04|02, $1, "," , " ", $3); } 
   ;

/*==========================================*/

list_item: T_AS   { $$ = $1; }
| T_PEERAS     { $$ = $1; }
| T_REAL       { $$ = $1; }
| T_PREFIX     { $$ = $1; }
| T_PREFIX6    { $$ = $1; }
| T_SPREFIX    { $$ = $1; }
| T_SPREFIX6   { $$ = $1; }
| T_DNAME opt_port { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); } 
| T_NUM opt_port { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); } 
| T_WORD       { $$ = $1; }
| '{' generic_list '}'  { $$ =  my_strcat (&curr_obj, 3, 01|04, "{", $2 , "}"); };

/*==========================================*/

opt_router_expression:      { $$ = NULL; }
        | router_expression { $$ = $1; };

opt_router_expression_with_at:   { $$ = NULL; }
        | T_AT router_expression
            { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

router_expression: router_expression T_OR router_expression_term
            { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | router_expression_term { $$ = $1; };

router_expression_term: router_expression_term T_AND router_expression_factor
            { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | router_expression_term T_EXCEPT router_expression_factor
            { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | router_expression_factor { $$ = $1; };

router_expression_factor: '(' router_expression ')'
            { $$ = my_strcat (&curr_obj, 3, 01|04, "(", $2, ")"); }
        | router_expression_operand { $$ = $1; };

router_expression_operand: v4orv6addr { $$ = $1; }
        | T_DNAME          { $$ = $1; }
        | T_RTRSNAME       { $$ = $1; };

opt_inject_expression: { $$ = NULL; }
        | T_UPON inject_expression
            { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); };

inject_expression: inject_expression T_OR inject_expression_term
            { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | inject_expression_term { $$ = $1; };

inject_expression_term: inject_expression_term T_AND inject_expression_factor
            { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | inject_expression_factor { $$ = $1; };

inject_expression_factor: '(' inject_expression ')'
            { $$ = my_strcat (&curr_obj, 3, 01|04, "(", $2, ")"); }
        | inject_expression_operand { $$ = $1; };

inject_expression_operand: T_STATIC { $$ = $1; }
        | T_HAVE_COMPONENTS '{' opt_filter_prefix_list '}'
            { $$ = my_strcat (&curr_obj, 5, 02|04|020, $1, " ", "{", $3, "}"); } 
        | T_EXCLUDE '{' opt_filter_prefix_list '}'
            { $$ = my_strcat (&curr_obj, 5, 02|04|020, $1, " ", "{", $3, "}"); };

/*==========================================*/

opt_atomic: { $$ = strdup (""); }
        | T_ATOMIC { $$ = $1; };

components_list: { $$ = NULL; }
        | filter 
               { $$ = $1; filter_count++; }
        | components_list T_PROTOCOL T_WORD filter 
               { if (find_protocol (&proto_ll, $3) == NULL) {
                   snprintf (err_buf, ERR_BUF_SIZE, "Unrecognized protocol \"%s\"", $3);
                   error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
                 }
                 $$ = my_strcat (&curr_obj, 7, 02|010|040, $1, " ", $2, " ", $3, " ",$4);  
                 filter_count++;
               };

filter: filter T_OR filter_term
               { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | filter filter_term
               { $$ = my_strcat (&curr_obj, 3, 02|010, $1, " ", $2); }
        | filter_term
               { $$ = $1; };

filter_term: filter_term T_AND filter_factor
               { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | filter_factor { $$ = $1; };

filter_factor: T_NOT filter_factor
                { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); }
        | '(' filter ')'
                { $$ = my_strcat (&curr_obj, 3, 01|04, "(", $2, ")"); }
        | filter_operand { $$ = $1; };

filter_operand: T_ANY  { $$ = $1; }
       | T_FLTRNAME    { $$ = $1; }
       | filter_prefix { $$ = $1; }
       | '<' filter_aspath '>'
          { $$ = my_strcat (&curr_obj, 3, 01|04, "<", $2, ">"); }
       | filter_rp_attribute { $$ = $1; };

filter_prefix: filter_prefix_operand T_OP_MS
              { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); }
       | filter_prefix_operand { $$ = $1; };

filter_prefix_operand: T_AS { $$ = $1; }
       | T_PEERAS           { $$ = $1; }
       | T_ASNAME           { $$ = $1; }
       | T_RSNAME           { $$ = $1; }
       | '{' opt_filter_prefix_list '}'
          { $$ = my_strcat (&curr_obj, 3, 01|04, "{", $2, "}"); };

filter_aspath: filter_aspath '|' filter_aspath_term
          { $$ = my_strcat (&curr_obj, 5, 02|04|010, $1, " ", "|", " ", $3); }
       | filter_aspath_term { $$ = $1; };

filter_aspath_term: filter_aspath_term filter_aspath_closure
          { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); }
       | filter_aspath_closure { $$ = $1; };

filter_aspath_closure: filter_aspath_closure '*'
          { $$ = my_strcat (&curr_obj, 2, 02, $1, "*"); }
       | filter_aspath_closure '?' 
          { $$ = my_strcat (&curr_obj, 2, 02, $1, "?"); }
       | filter_aspath_closure '+'     
          { $$ = my_strcat (&curr_obj, 2, 02, $1, "+"); }
       | filter_aspath_closure T_OP_TILDA    
          { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); }
       | filter_aspath_closure T_REGEX_RANGE    
          { $$ = my_strcat (&curr_obj, 2, 0, $1, $2); }
       | filter_aspath_factor { $$ = $1; };

filter_aspath_factor: '^' { $$ = strdup ("^"); }
       | '$'              { $$ = strdup ("$"); }               
       | '(' filter_aspath ')'
          { $$ = my_strcat (&curr_obj, 3, 01|04, "(", $2, ")"); }
       | filter_aspath_no { $$ = $1; };

filter_aspath_no: T_AS { $$ = $1; }
       | T_PEERAS      { $$ = $1; }
       | T_ASNAME      { $$ = $1; }
       | '.'           { $$ = strdup ("."); }
       | '[' filter_aspath_range ']'
          { $$ = my_strcat (&curr_obj, 3, 01|04, "[", $2, "]"); }
       ;
/*JW is "filter_aspath_range '.'" correct?  should it be '...'? */
filter_aspath_range: { $$ = NULL; }
       | filter_aspath_range T_AS     { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); }
       | filter_aspath_range T_PEERAS { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); }
       | filter_aspath_range '.'      { $$ = my_strcat (&curr_obj, 3, 02|04, $1, " ", "."); }
       | filter_aspath_range T_AS '-' T_AS
          { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, "-", $4); }
       | filter_aspath_range T_ASNAME { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); }
       ;

filter_rp_attribute: T_RP_ATTR '.' T_WORD '(' generic_list ')'
     { 
       fprintf (dfile, "\n\n++++++++++++\n rp_attr=(%s) word=(%s) list_item=(%s)\n", $1, $3, $5);
       rp = is_RPattr (&rp_attr_ll, $1);
       if ( rp == NULL || (method = find_method (rp->first, $3)) == NULL) {
         snprintf (err_buf, ERR_BUF_SIZE,
	   "\"%s\" is an illegal RPSL method for RP attribute \"%s\"", $3, $1);
	 error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
       }
       else if (!valid_args (&curr_obj, method, 1, $1, 1, &$5))
	 curr_obj.error_pos = tokenpos;
       $$ = my_strcat (&curr_obj, 6, 02|010|040, $1, ".", $3, "(", $5, ")");
      }
   | T_RP_ATTR T_OP list_item
     { 
       fprintf (dfile, "\n\n++++++++++++\n rp_attr=(%s) op=(%s) list_item=(%s)\n", $1, $2, $3);
       rp = is_RPattr (&rp_attr_ll, $1);
       if ( rp == NULL || (method = find_method (rp->first, $2)) == NULL) {
	 snprintf (err_buf, ERR_BUF_SIZE,
	   "\"%s\" is an illegal RPSL operator for RP attribute \"%s\"", $2, $1);
	 error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
       }
       else if (!valid_args (&curr_obj, method, 0, $1, 1, &$3))
	 curr_obj.error_pos = tokenpos;
       $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3);
     }
   | T_RP_ATTR '(' generic_list ')'
     { 
       fprintf (dfile, "\n\n++++++++++++\n rp_attr=(%s) generic_list=(%s)\n", $1, $3);
       rp = is_RPattr (&rp_attr_ll, $1);
       if ( rp == NULL || (method = find_method (rp->first, "()")) == NULL) {
	 snprintf (err_buf, ERR_BUF_SIZE,
	   "\"()\" is an illegal RPSL operator for RP attribute \"%s\"", $1);
	 error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
       }
       else if (!valid_args (&curr_obj, method, 1, $1, 1, &$3))
	 curr_obj.error_pos = tokenpos;
       $$ = my_strcat (&curr_obj, 4, 02|010, $1, "(", $3, ")"); 
     }
   | T_RP_ATTR '[' generic_list ']'
     { rp = is_RPattr (&rp_attr_ll, $1);
       if ( rp == NULL || (method = find_method (rp->first, "[]")) == NULL) {
	 snprintf (err_buf, ERR_BUF_SIZE,
	   "\"[]\" is an illegal RPSL operator for RP attribute \"%s\"", $1);
	 error_msg_queue (&curr_obj, err_buf, ERROR_MSG);
       }
       else if (!valid_args (&curr_obj, method, 1, $1, 1, &$3))
	 curr_obj.error_pos = tokenpos;
       $$ = my_strcat (&curr_obj, 4, 02|010, $1, "[", $3, "]"); 
     }
   ; 

opt_filter_prefix_list: { $$ = strdup (""); }
	  | filter_prefix_list { $$ = $1; };

filter_prefix_list: filter_prefix_list_prefix { $$ = $1; }
          | filter_prefix_list ',' filter_prefix_list_prefix
              { $$ = my_strcat (&curr_obj, 4, 02|04, $1, "," , " ", $3); };

filter_prefix_list_prefix: T_PREFIX { $$ = $1; 
      if (curr_obj.type == O_R6)
        error_msg_queue (&curr_obj, "IPv4 prefixes not allowed in attributes of route6 class", ERROR_MSG); }
	  | T_PREFIX6		    { $$ = $1;
      if (curr_obj.type != O_R6 && (curr_obj.type == O_RT || mp_attr == 0) )
        error_msg_queue (&curr_obj, "IPv6 prefixes not permitted", ERROR_MSG); }
          | T_PREFIXRNG             { $$ = $1;
      if (curr_obj.type == O_R6)
        error_msg_queue (&curr_obj, "IPv4 prefixes not allowed in attributes of route6 class", ERROR_MSG); }
          | T_PREFIXRNG6            { $$ = $1;
      if (curr_obj.type != O_R6 && (curr_obj.type == O_RT || mp_attr == 0) )
        error_msg_queue (&curr_obj, "IPv6 prefixes not permitted", ERROR_MSG); };

aggr_mtd_attr: T_INBOUND               { $$ = $1; }
        | T_OUTBOUND opt_as_expression { $$ = my_strcat (&curr_obj, 3, 02, $1, " ", $2); }
        ;

opt_as_expression:      { $$ = NULL; }
        | T_AS_ANY      { $$ = $1; }
	| as_expression { $$ = $1; };

as_expression: as_expression T_OR as_expression_term 
          { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | as_expression_term { $$ = $1; };

as_expression_term: as_expression_term T_AND as_expression_factor
          { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | as_expression_term T_EXCEPT as_expression_factor 
          { $$ = my_strcat (&curr_obj, 5, 02|010, $1, " ", $2, " ", $3); }
        | as_expression_factor { $$ = $1; };

as_expression_factor: '(' as_expression ')' 
          { $$ = my_strcat (&curr_obj, 3, 01|04, "(", $2, ")"); }
        | as_expression_operand { $$ = $1; };
        
as_expression_operand: T_AS { $$ = $1; }
        | T_ASNAME          { $$ = $1; };

%%

/*
 * We've parsed a line, now get ready for the next line.
 * curr_obj.curr_attr saves the type of attribute for
 * the curent line.
 * parse_buf is the storage area for canonicalized objects.
 * bufp points to the next available location in parse_buf.
 */
void start_new_attr () { 
  char buf[2048];
  int i, j, k;
  long fpos;
  canon_line_t lrec;

  start_new_line = 1;
  reset_token_buffer ();
  /* 
   *
   * There are 5 possible outcomes for a line:
   *   SYNTAX_ERROR
   *   EMPTY_LINE
   *   LEGAL_LINE       ie, no errors and not empty 
   *   DEL_LINE         ie, password and cookie fields; treat
   *                    line as if it never existed; these
   *                    fields are informational only
   *   MACHINE_GEN_LINE ignore user input and use program
   *                    generated value
   */

  if (parse_RPSL_dictionary)
    curr_obj.attr_error = DEL_LINE;

  if (curr_obj.attr_error & EMPTY_LINE) {
    set_skip_attr (&canon_obj, &curr_obj);
    curr_obj.elines = 0;
  }
  else if (curr_obj.attr_error & LEGAL_LINE) {
    if (curr_obj.attr_too_big ||
	(curr_obj.curr_attr != F_CE &&
	(curr_obj.elines > 1        ||
	 curr_obj.curr_attr == F_DE  ||
	 curr_obj.curr_attr == F_RM  ||
	 curr_obj.curr_attr == F_ST  ||
	 curr_obj.curr_attr == F_PN  ||
	 curr_obj.curr_attr == F_RO  ||
	 curr_obj.curr_attr == F_TB  ||
	 curr_obj.curr_attr == F_AD  ||
	 curr_obj.curr_attr == F_PH  ||
	 curr_obj.curr_attr == F_FX  ||
	 parse_RPSL_dictionary)))
      rpsl_lncont (&curr_obj, &canon_obj, 0);
  }
  else if (curr_obj.attr_error & MACHINE_GEN_LINE) {
    set_skip_attr (&canon_obj, &curr_obj);
    rpsl_lncont (&curr_obj, &canon_obj, 0);
  }

  if (!(curr_obj.attr_error & DEL_LINE))
    curr_obj.num_lines++;

  curr_obj.start_lineno += curr_obj.elines;
  curr_obj.curr_attr     = F_NOATTR;
  curr_obj.field_count   = 0;
  curr_obj.attr_error    = LEGAL_LINE;

  if (!(curr_obj.attr_error & DEL_LINE))
      start_new_canonical_line (&canon_obj, &curr_obj);

  ERROR_STATE = 0;

  if (!canon_obj.do_canon)
     return;

  fprintf (dfile, "start_new_attr (): start_line (%d) num_lines (%d) eline_len (%d) elines (%d)\n", curr_obj.start_lineno, curr_obj.num_lines, curr_obj.eline_len, curr_obj.elines);
  
  if (canon_obj.io == CANON_MEM) {
    if (canon_obj.lio == CANON_MEM) {
      for (i = 0; i < curr_obj.num_lines; i++)
	fputs (lineptr[i].ptr, dfile);
    }
    else {
      fseek (canon_obj.lfd, 0, SEEK_SET);
      for (i = 0; i <  curr_obj.num_lines; i++) {
	if (!fread (&lrec, sizeof (canon_line_t), 1, canon_obj.lfd))
	  fprintf (dfile, "error reading disk line rec #(%d)\n", i);
	else
	  fputs (lrec.ptr, dfile);
      }      
    }
  }
  
  if (canon_obj.io == CANON_DISK) {
    fpos = ftell (canon_obj.fd);
    if (canon_obj.lio == CANON_MEM) { 
      for (i = 0; i < curr_obj.num_lines; i++) {
	fseek (canon_obj.fd, lineptr[i].fpos, SEEK_SET);
	fprintf (dfile, "fpos (%ld):", lineptr[i].fpos);
	k = 0;
	do {
	  if (fgets (buf, 2048, canon_obj.fd) == NULL)
	    break;
	  j = strlen (buf) - 1;
	  if (j < 0)
	    break;
	  fputs (buf, dfile);
	  if (buf[j] == '\n')
	    k++;
	} while ((lineptr[i].lines - k) > 0);
      }
      fprintf (dfile, "fpos (%ld):\n", ftell (canon_obj.fd));
    }
    else {
      fseek (canon_obj.lfd, 0, SEEK_SET);
      for (i = 0; i < curr_obj.num_lines; i++) {
	fread (&lrec, sizeof (canon_line_t), 1, canon_obj.lfd);
	fprintf (dfile, "fpos (%ld):", lrec.fpos);
	fseek (canon_obj.fd, lrec.fpos, SEEK_SET);
	k = 0;
	do {
	  if (fgets (buf, 2048, canon_obj.fd) == NULL)
	    break;
	  j = strlen (buf) - 1;
	  if (j < 0)
	    break;
	  fputs (buf, dfile);
	  if (buf[j] == '\n')
	    k++;
	} while ((lrec.lines - k) > 0);
      }
      fprintf (dfile, "fpos (%ld):\n", ftell (canon_obj.fd));   
    }
    fseek (canon_obj.fd, fpos, SEEK_SET);
    fprintf (dfile, "final fpos (%ld):\n", ftell (canon_obj.fd));
  }
}

