#include "scan.h"

/* obj_template [] uses the 'enum IRR_OBJECTS' data struct defined
 * in scan.h as it's index.  Any changes to obj_template [] or
 * 'enum IRR_OBJECTS' need to be coordinated.
 *
 * The values/elements of this array were generated with the 
 * 'create_templates.pl' which is in the irr_util directory.
 * The script uses the file 'templates.config' (also in irr_util)
 * as input.  'templates.config' is an ascii file in which object
 * definitions can be added, deleted, or modified easily.
 *
 * To add a new object type/template, first update the 'templates.config' file
 * and then run 'create_templates.pl' to create the new templates and 
 * replace below.  'enum IRR_OBJECTS' in scan.h needs to be updated 
 * and m_info [] in commands.c and key_info [] in scan.c need to be 
 * udpated also (see 'enum IRR_OBJECTS' in scan.h for more information). 
 */
char *obj_template[IRR_MAX_KEYS] = {
"aut-num:     [mandatory]   [single]     [primary/look-up key]\n"
"as-name:     [optional]    [single]     [ ]\n"
"descr:       [mandatory]   [single]     [ ]\n"
"admin-c:     [mandatory]   [multiple]   [ ]\n"
"tech-c:      [mandatory]   [multiple]   [ ]\n"
"member-of:   [optional]    [multiple]   [ ]\n"
"import:      [optional]    [multiple]   [ ]\n"
"export:      [optional]    [multiple]   [ ]\n"
"default:     [optional]    [multiple]   [ ]\n"
"remarks:     [optional]    [multiple]   [ ]\n"
"notify:      [optional]    [multiple]   [ ]\n"
"mnt-by:      [mandatory]   [multiple]   [ ]\n"
"changed:     [mandatory]   [multiple]   [ ]\n"
"source:      [mandatory]   [single]     [ ]\n",

"route:          [mandatory]   [single]     [primary/look-up key]\n"
"descr:          [mandatory]   [single]     [ ]\n"
"admin-c:        [optional]    [multiple]   [ ]\n"
"tech-c:         [optional]    [multiple]   [ ]\n"
"origin:         [mandatory]   [single]     [primary key]\n"
"holes:          [optional]    [multiple]   [ ]\n"
"member-of:      [optional]    [multiple]   [ ]\n"
"inject:         [optional]    [multiple]   [ ]\n"
"components:     [optional]    [single]     [ ]\n"
"aggr-bndry:     [optional]    [single]     [ ]\n"
"aggr-mtd:       [optional]    [single]     [ ]\n"
"export-comps:   [optional]    [single]     [ ]\n"
"remarks:        [optional]    [multiple]   [ ]\n"
"notify:         [optional]    [multiple]   [ ]\n"
"mnt-by:         [mandatory]   [multiple]   [ ]\n"
"changed:        [mandatory]   [multiple]   [ ]\n"
"source:         [mandatory]   [single]     [ ]\n",

"as-macro:   [mandatory]   [single]     [primary/look-up key]\n"
"descr:      [mandatory]   [multiple]   [ ]\n"
"as-list:    [mandatory]   [multiple]   [ ]\n"
"guardian:   [optional]    [single]     [ ]\n"
"tech-c:     [mandatory]   [multiple]   [ ]\n"
"admin-c:    [mandatory]   [multiple]   [ ]\n"
"remarks:    [optional]    [multiple]   [ ]\n"
"notify:     [optional]    [multiple]   [ ]\n"
"mnt-by:     [mandatory]   [multiple]   [ ]\n"
"changed:    [mandatory]   [multiple]   [ ]\n"
"source:     [mandatory]   [single]     [ ]\n",

"community:   [mandatory]   [single]     [primary/look-up key]\n"
"descr:       [mandatory]   [multiple]   [ ]\n"
"authority:   [mandatory]   [single]     [ ]\n"
"guardian:    [optional]    [single]     [ ]\n"
"tech-c:      [mandatory]   [multiple]   [ ]\n"
"admin-c:     [mandatory]   [multiple]   [ ]\n"
"remarks:     [optional]    [multiple]   [ ]\n"
"notify:      [optional]    [multiple]   [ ]\n"
"mnt-by:      [mandatory]   [multiple]   [ ]\n"
"changed:     [mandatory]   [multiple]   [ ]\n"
"source:      [mandatory]   [single]     [ ]\n",

"mntner:    [mandatory]   [single]     [primary/look-up key]\n"
"descr:     [mandatory]   [single]     [ ]\n"
"admin-c:   [optional]    [multiple]   [ ]\n"
"tech-c:    [mandatory]   [multiple]   [ ]\n"
"upd-to:    [mandatory]   [multiple]   [ ]\n"
"mnt-nfy:   [optional]    [multiple]   [ ]\n"
"auth:      [mandatory]   [multiple]   [ ]\n"
"remarks:   [optional]    [multiple]   [ ]\n"
"notify:    [optional]    [multiple]   [ ]\n"
"mnt-by:    [mandatory]   [multiple]   [ ]\n"
"changed:   [mandatory]   [multiple]   [ ]\n"
"source:    [mandatory]   [single]     [ ]\n",

"inet-rtr:   [mandatory]   [single]     [primary/look-up key]\n"
"descr:      [optional]    [single]     [ ]\n"
"alias:      [optional]    [multiple]   [ ]\n"
"local-as:   [mandatory]   [single]     [ ]\n"
"ifaddr:     [mandatory]   [multiple]   [ ]\n"
"peer:       [optional]    [multiple]   [ ]\n"
"rs-in:      [optional]    [single]     [ ]\n"
"rs-out:     [optional]    [single]     [ ]\n"
"admin-c:    [optional]    [multiple]   [ ]\n"
"tech-c:     [mandatory]   [multiple]   [ ]\n"
"remarks:    [optional]    [multiple]   [ ]\n"
"notify:     [optional]    [multiple]   [ ]\n"
"mnt-by:     [mandatory]   [multiple]   [ ]\n"
"changed:    [mandatory]   [multiple]   [ ]\n"
"source:     [mandatory]   [single]     [ ]\n",

"person:    [mandatory]   [single]     [primary/look-up key]\n"
"address:   [mandatory]   [multiple]   [ ]\n"
"phone:     [mandatory]   [multiple]   [ ]\n"
"fax-no:    [optional]    [multiple]   [ ]\n"
"e-mail:    [optional]    [multiple]   [ ]\n"
"nic-hdl:   [mandatory]   [single]     [ ]\n"
"remarks:   [optional]    [multiple]   [ ]\n"
"notify:    [optional]    [multiple]   [ ]\n"
"mnt-by:    [mandatory]   [multiple]   [ ]\n"
"changed:   [mandatory]   [multiple]   [ ]\n"
"source:    [mandatory]   [single]     [ ]\n",

"role:      [mandatory]   [single]     [primary/look-up key]\n"
"address:   [mandatory]   [multiple]   [ ]\n"
"phone:     [mandatory]   [multiple]   [ ]\n"
"fax-no:    [optional]    [multiple]   [ ]\n"
"e-mail:    [mandatory]   [multiple]   [ ]\n"
"trouble:   [optional]    [multiple]   [ ]\n"
"nic-hdl:   [mandatory]   [single]     [ ]\n"
"admin-c:   [optional]    [multiple]   [ ]\n"
"tech-c:    [mandatory]   [multiple]   [ ]\n"
"remarks:   [optional]    [multiple]   [ ]\n"
"notify:    [optional]    [multiple]   [ ]\n"
"mnt-by:    [mandatory]   [multiple]   [ ]\n"
"changed:   [mandatory]   [multiple]   [ ]\n"
"source:    [mandatory]   [single]     [ ]\n",

"%  No template available\n\n",
"%  No template available\n\n",

"inetnum:   [mandatory]   [single]     [primary/look-up key]\n"
"netname:   [mandatory]   [single]     [ ]\n"
"descr:     [mandatory]   [single]     [ ]\n"
"country:   [mandatory]   [multiple]   [ ]\n"
"admin-c:   [mandatory]   [multiple]   [ ]\n"
"tech-c:    [mandatory]   [multiple]   [ ]\n"
"aut-sys:   [optional]    [single]     [ ]\n"
"ias-int:   [optional]    [single]     [ ]\n"
"gateway:   [optional]    [single]     [ ]\n"
"rev-srv:   [optional]    [multiple]   [ ]\n"
"status:    [mandatory]   [single]     [ ]\n"
"remarks:   [optional]    [multiple]   [ ]\n"
"notify:    [optional]    [multiple]   [ ]\n"
"mnt-by:    [mandatory]   [multiple]   [ ]\n"
"changed:   [mandatory]   [multiple]   [ ]\n"
"source:    [mandatory]   [single]     [ ]\n",

"%  No template available\n\n",
"%  No template available\n\n",
"%  No template available\n\n",
"%  No template available\n\n",
"%  No template available\n\n",
"%  No template available\n\n",
"%  No template available\n\n",

"domain:    [mandatory]   [single]     [primary/look-up key]\n"
"descr:     [mandatory]   [single]     [ ]\n"
"admin-c:   [mandatory]   [multiple]   [ ]\n"
"tech-c:    [mandatory]   [multiple]   [ ]\n"
"zone-c:    [mandatory]   [multiple]   [ ]\n"
"nserver:   [optional]    [multiple]   [ ]\n"
"sub-dom:   [optional]    [multiple]   [ ]\n"
"dom-net:   [optional]    [multiple]   [ ]\n"
"remarks:   [optional]    [multiple]   [ ]\n"
"notify:    [optional]    [multiple]   [ ]\n"
"mnt-by:    [optional]    [multiple]   [ ]\n"
"refer:     [optional]    [single]     [ ]\n"
"changed:   [mandatory]   [multiple]   [ ]\n"
"source:    [mandatory]   [single]     [ ]\n",

"%  No template available\n\n",
"%  No template available\n\n",
"%  No template available\n\n",
"%  No template available\n\n",
"%  No template available\n\n",
"%  No template available\n\n",

"as-set:        [mandatory]   [single]     [primary/look-up key]\n"
"descr:         [mandatory]   [single]     [ ]\n"
"members:       [optional]    [multiple]   [ ]\n"
"mbrs-by-ref:   [optional]    [multiple]   [ ]\n"
"remarks:       [optional]    [multiple]   [ ]\n"
"admin-c:       [optional]    [multiple]   [ ]\n"
"tech-c:        [mandatory]   [multiple]   [ ]\n"
"notify:        [optional]    [multiple]   [ ]\n"
"mnt-by:        [mandatory]   [multiple]   [ ]\n"
"changed:       [mandatory]   [multiple]   [ ]\n"
"source:        [mandatory]   [single]     [ ]\n",

"route-set:     [mandatory]   [single]     [primary/look-up key]\n"
"descr:         [mandatory]   [single]     [ ]\n"
"members:       [optional]    [multiple]   [ ]\n"
"mbrs-by-ref:   [optional]    [multiple]   [ ]\n"
"remarks:       [optional]    [multiple]   [ ]\n"
"admin-c:       [optional]    [multiple]   [ ]\n"
"tech-c:        [mandatory]   [multiple]   [ ]\n"
"notify:        [optional]    [multiple]   [ ]\n"
"mnt-by:        [mandatory]   [multiple]   [ ]\n"
"changed:       [mandatory]   [multiple]   [ ]\n"
"source:        [mandatory]   [single]     [ ]\n",

"%  No template available\n\n",

"filter-set:   [mandatory]   [single]     [primary/look-up key]\n"
"filter:       [mandatory]   [single]     [ ]\n"
"descr:        [mandatory]   [single]     [ ]\n"
"remarks:      [optional]    [multiple]   [ ]\n"
"admin-c:      [optional]    [multiple]   [ ]\n"
"tech-c:       [mandatory]   [multiple]   [ ]\n"
"notify:       [optional]    [multiple]   [ ]\n"
"mnt-by:       [mandatory]   [multiple]   [ ]\n"
"changed:      [mandatory]   [multiple]   [ ]\n"
"source:       [mandatory]   [single]     [ ]\n",

"rtr-set:       [mandatory]   [single]     [primary/look-up key]\n"
"descr:         [mandatory]   [single]     [ ]\n"
"members:       [optional]    [multiple]   [ ]\n"
"mbrs-by-ref:   [optional]    [multiple]   [ ]\n"
"remarks:       [optional]    [multiple]   [ ]\n"
"admin-c:       [optional]    [multiple]   [ ]\n"
"tech-c:        [mandatory]   [multiple]   [ ]\n"
"notify:        [optional]    [multiple]   [ ]\n"
"mnt-by:        [mandatory]   [multiple]   [ ]\n"
"changed:       [mandatory]   [multiple]   [ ]\n"
"source:        [mandatory]   [single]     [ ]\n",

"peering-set:   [mandatory]   [single]     [primary/look-up key]\n"
"peering:       [mandatory]   [multiple]   [ ]\n"
"descr:         [mandatory]   [single]     [ ]\n"
"remarks:       [optional]    [multiple]   [ ]\n"
"admin-c:       [optional]    [multiple]   [ ]\n"
"tech-c:        [mandatory]   [multiple]   [ ]\n"
"notify:        [optional]    [multiple]   [ ]\n"
"mnt-by:        [mandatory]   [multiple]   [ ]\n"
"changed:       [mandatory]   [multiple]   [ ]\n"
"source:        [mandatory]   [single]     [ ]\n",

"key-cert:   [mandatory]   [single]     [primary/look-up key]\n"
"method:     [generated]   [single]     [ ]\n"
"owner:      [generated]   [multiple]   [ ]\n"
"fingerpr:   [generated]   [single]     [ ]\n"
"certif:     [mandatory]   [single]     [ ]\n"
"remarks:    [optional]    [multiple]   [ ]\n"
"notify:     [optional]    [multiple]   [ ]\n"
"mnt-by:     [mandatory]   [multiple]   [ ]\n"
"changed:    [mandatory]   [multiple]   [ ]\n"
"source:     [mandatory]   [single]     [ ]\n",

"dictionary:      [mandatory]   [single]     [primary/look-up key]\n"
"descr:           [mandatory]   [single]     [ ]\n"
"admin-c:         [optional]    [multiple]   [ ]\n"
"tech-c:          [mandatory]   [multiple]   [ ]\n"
"typedef:         [optional]    [multiple]   [ ]\n"
"rp-attribute:    [optional]    [multiple]   [ ]\n"
"protocol:        [optional]    [multiple]   [ ]\n"
"encapsulation:   [optional]    [multiple]   [ ]\n"
"remarks:         [optional]    [multiple]   [ ]\n"
"notify:          [optional]    [multiple]   [ ]\n"
"mnt-by:          [mandatory]   [multiple]   [ ]\n"
"changed:         [mandatory]   [multiple]   [ ]\n"
"source:          [mandatory]   [single]     [ ]\n",

"repository:           [mandatory]   [single]     [primary/look-up key]\n"
"query-address:        [mandatory]   [multiple]   [ ]\n"
"response-auth-type:   [mandatory]   [multiple]   [ ]\n"
"submit-address:       [mandatory]   [multiple]   [ ]\n"
"submit-auth-type:     [mandatory]   [multiple]   [ ]\n"
"repository-cert:      [mandatory]   [multiple]   [ ]\n"
"expire:               [mandatory]   [single]     [ ]\n"
"heartbeat-interval:   [mandatory]   [single]     [ ]\n"
"reseed-address:       [mandatory]   [multiple]   [ ]\n"
"descr:                [mandatory]   [single]     [ ]\n"
"remarks:              [optional]    [multiple]   [ ]\n"
"admin-c:              [optional]    [multiple]   [ ]\n"
"tech-c:               [mandatory]   [multiple]   [ ]\n"
"notify:               [optional]    [multiple]   [ ]\n"
"mnt-by:               [mandatory]   [multiple]   [ ]\n"
"changed:              [mandatory]   [multiple]   [ ]\n"
"source:               [mandatory]   [single]     [ ]\n",

"inet6num:   [mandatory]   [single]     [primary/look-up key]\n"
"netname:    [mandatory]   [single]     [ ]\n"
"descr:      [mandatory]   [single]     [ ]\n"
"country:    [mandatory]   [multiple]   [ ]\n"
"admin-c:    [mandatory]   [multiple]   [ ]\n"
"tech-c:     [mandatory]   [multiple]   [ ]\n"
"rev-srv:    [optional]    [multiple]   [ ]\n"
"remarks:    [optional]    [multiple]   [ ]\n"
"notify:     [optional]    [multiple]   [ ]\n"
"mnt-by:     [mandatory]   [multiple]   [ ]\n"
"changed:    [mandatory]   [multiple]   [ ]\n"
"source:     [mandatory]   [single]     [ ]\n",

"dom-prefix:   [mandatory]   [single]     [primary/look-up key]\n"
"dom-name:     [mandatory]   [single]     [ ]\n"
"descr:        [optional]    [single]     [ ]\n"
"bis:          [optional]    [multiple]   [ ]\n"
"dom-in:       [optional]    [multiple]   [ ]\n"
"dom-out:      [optional]    [multiple]   [ ]\n"
"default:      [optional]    [multiple]   [ ]\n"
"admin-c:      [mandatory]   [multiple]   [ ]\n"
"tech-c:       [mandatory]   [multiple]   [ ]\n"
"guardian:     [optional]    [single]     [ ]\n"
"remarks:      [optional]    [multiple]   [ ]\n"
"notify:       [optional]    [multiple]   [ ]\n"
"mnt-by:       [mandatory]   [multiple]   [ ]\n"
"changed:      [mandatory]   [multiple]   [ ]\n"
"source:       [mandatory]   [single]     [ ]\n"
};
