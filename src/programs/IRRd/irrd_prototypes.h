/* 
 * $Id: irrd_prototypes.h,v 1.26 2002/10/17 20:02:30 ljb Exp $
 * originally Id: irrd_prototypes.h,v 1.46 1998/08/03 17:29:08 gerald Exp 
 */

/* char *my_fgets (irr_database_t *db, FILE *fp); */

/* update */
int add_irr_object (irr_database_t *database, irr_object_t *irr_object);
irr_object_t *load_irr_object (irr_database_t *database, irr_object_t *irr_object);
int delete_irr_object (irr_database_t *database, irr_object_t *irr_object,
                       u_long *db_offset);
void mark_deleted_irr_object (irr_database_t *database, u_long offset);

/* configuration */
int config_irr_database_nodefault (uii_connection_t *uii, char *name);
int config_irr_database_routing_table_dump (uii_connection_t *uii, char *name);
int config_irr_database_clean (uii_connection_t *uii, char *name, int seconds);
int config_irr_database_no_clean (uii_connection_t *uii, char *name);
int config_tmp_directory (uii_connection_t *uii, char *dir);
int config_irr_database_export (uii_connection_t *uii, char *name, int interval, int n, char *filename);
int config_export_directory (uii_connection_t *uii, char *dir);
int config_debug_submission_syslog (uii_connection_t *uii);
int config_debug_submission_maxsize (uii_connection_t *uii, int size);
int config_debug_submission_verbose (uii_connection_t *uii);
int config_debug_server_verbose (uii_connection_t *uii);
int config_no_debug_server_verbose (uii_connection_t *uii);
int config_debug_server_file (uii_connection_t *uii, char *filename);
int no_config_debug_server (uii_connection_t *uii);
int config_debug_submission_file (uii_connection_t *uii, char *filename);
int no_config_debug_submission (uii_connection_t *uii);
int config_debug_server_size (uii_connection_t *uii, int bytes);
void get_config_irr_port ();
int config_override (uii_connection_t *uii, char *override);
int config_pgpdir (uii_connection_t *uii, char *pgpdir);
int config_irr_host (uii_connection_t *uii, char *host);
int config_irr_directory (uii_connection_t *uii, char *directory);
int config_irr_database_mirror (uii_connection_t *uii, char *name,
				char *host, int port);
int config_dbadmin (uii_connection_t *uii, char *email);
int config_debug_server_syslog (uii_connection_t *uii);
int config_irr_database (uii_connection_t *uii, char *name);
int config_irr_database_authoritative (uii_connection_t *uii, char *name);
int no_config_irr_database_authoritative (uii_connection_t *uii, char *name);
int no_config_irr_database (uii_connection_t *uii, char *name);
int config_irr_expansion_timeout (uii_connection_t *uii, int timeout);
int config_irr_max_con (uii_connection_t *uii, int max);
void config_create_default ();
void get_config_irr_directory ();
int config_irr_database_access_write (uii_connection_t *uii, char *name, int num);
int config_irr_database_access_cryptpw (uii_connection_t *uii, char *name, int num);
int config_irr_database_mirror_protocol (uii_connection_t *uii, char *name, int num);
int config_irr_database_mirror_access (uii_connection_t *uii, char *name, int num);
int config_irr_database_compress_script (uii_connection_t *uii, char *name,  char *script);
int config_irr_database_access (uii_connection_t *uii, char *name, int num);
int no_config_irr_dbclean (uii_connection_t *uii);
int config_irr_dbclean (uii_connection_t *uii);
int config_irr_database_filter (uii_connection_t *uii, char *name, char *object);
int config_db_ignore (uii_connection_t *uii, char *str);
int config_response_footer (uii_connection_t *uii, char *line);
int config_response_notify_header (uii_connection_t *uii, char *line);
int config_response_forward_header (uii_connection_t *uii, char *line);
int config_irr_remote_ftp_url (uii_connection_t *uii, char *name, char *dir);
int config_irr_path (uii_connection_t *uii, char *path);
int config_irr_repos_hexid (uii_connection_t *, char *, char *);
int config_irr_pgppass (uii_connection_t *, char *, char *);
int config_rpsdist_database (uii_connection_t *, char *);
int config_rpsdist_database_trusted (uii_connection_t *, char *, char *, int);
int config_rpsdist_database_authoritative (uii_connection_t *, char *, char *);
int config_rpsdist_accept_database (uii_connection_t *, char *, char *);
int config_rpsdist_accept (uii_connection_t *, char *);

/* uii */
int kill_irrd (uii_connection_t *uii);
int uii_delete_route (uii_connection_t *uii, char *database, prefix_t *prefix, int as);
void config_irr_mirror_interval (uii_connection_t *uii, int interval);
void uii_irr_reload (uii_connection_t *uii, char *name);
int  uii_irr_irrdcacher (uii_connection_t *, char *);
void uii_irr_mirror (uii_connection_t *uii, char *name, uint32_t serial);
void uii_irr_mirror_last (uii_connection_t *uii, char *name);
void uii_irr_mirror_serial (uii_connection_t *uii, char *name, uint32_t serial);
void uii_mirrorstatus_db (uii_connection_t *uii, char *db_name);
void uii_set_serial (uii_connection_t *uii, char *name, uint32_t serial);
void show_database (uii_connection_t *uii);
void uii_show_ip (uii_connection_t *uii, prefix_t *prefix, int num, char *lessmore);
void uii_irr_clean (uii_connection_t *uii, char *name);
void config_irr_port   (uii_connection_t *, int, int);
void config_irr_port_2 (uii_connection_t *, int);
void uii_export_database (uii_connection_t *uii, char *name);
int uii_read_update_file (uii_connection_t *uii, char *file, char *name);

/* telnet */
void irr_write  (irr_connection_t *irr, char *buf, int len);
void irr_write_buffer_flush (irr_connection_t *irr);
void irr_write_nobuffer (irr_connection_t *irr, char *buf);
void irr_send_answer (irr_connection_t * irr);
int irr_add_answer (irr_connection_t *irr, char *format, ...);
void irr_write_error (irr_connection_t *irr);
void irr_send_okay (irr_connection_t * irr);
void irr_send_error (irr_connection_t * irr, char *);
void irr_mode_send_error (irr_connection_t * irr, int mode, char *);
void irr_build_memory_answer (irr_connection_t *irr, u_long len, char * blob);
void irr_build_answer (irr_connection_t *irr, irr_database_t *database, enum IRR_OBJECTS type, u_long offset, u_long len);
void send_dbobjs_answer (irr_connection_t * irr, enum INDEX_T index, int mode);
int listen_telnet (u_short port);
int irr_destroy_connection (irr_connection_t * connection);
void show_connections (uii_connection_t *uii);

/* database */
int irr_load_data (int, int);
int irr_copy_file (char *infile, char *outfile, int add_eof_flag);
void database_clear (irr_database_t *db);
int irr_reload_database (char *names, uii_connection_t *uii, char *tmp_dir);
int irr_database_clean (irr_database_t *database);
int irr_database_export (irr_database_t *database);
void irr_export_timer (mtimer_t *timer, irr_database_t *db);

/* journaling */
void journal_maybe_rollover (irr_database_t *database);
void journal_log_serial_number (irr_database_t *database);
void journal_irr_update (irr_database_t *db, irr_object_t *object,
                         int mode, int skip_obj);
int find_oldest_serial (char *dbname, int journal_ext, uint32_t *oldestserial);
int find_last_serial (char *dbname, int journal_ext, uint32_t *last_serial);
int get_current_serial (char *dbname, uint32_t *currserial);
void make_journal_name (char * dbname, int journal_ext, char * journal_name);

/* util */
void irr_sort_database ();
void nice_time (long seconds, char *buf);
long copy_irr_object ( irr_database_t *database, irr_object_t *object);
void irr_unlock_all (irr_connection_t *irr);
void irr_lock_all (irr_connection_t *irr);
void irr_update_unlock (irr_database_t *database);
void irr_update_lock (irr_database_t *database);
void irr_clean_unlock (irr_database_t *database);
void irr_clean_lock (irr_database_t *database);
void irr_unlock (irr_database_t *database);
void irr_lock (irr_database_t *database);
irr_database_t *find_database (char *name);
irr_database_t *new_database (char *name);
irr_object_t *New_IRR_Object (char *buffer, u_long position, u_long mode);
void Delete_IRR_Object (irr_object_t *object);
void Delete_Ref_keys (reference_key_t *ref_item);
void lookup_object_references (irr_connection_t *irr);
void lookup_prefix_exact (irr_connection_t *irr, char *key, enum IRR_OBJECTS type);
int convert_to_32 (char *strval, uint32_t *uval);
irr_hash_string_t *new_irr_hash_string (char *str);
void delete_irr_hash_string (irr_hash_string_t *str);
int parse_ripe_flags (irr_connection_t *irr, char **cp);
int ripe_set_source (irr_connection_t *irr, char **cp);
int ripe_obj_type (irr_connection_t *irr, char **cp);
void radix_flush (radix_tree_t *radix_tree);
void interactive_io (char *);
void scrub_cryptpw (char *);
char *print_as (char *, uint32_t);
char *dir_chks (char *, int);

/* mirroring */
void irr_mirror_timer (mtimer_t *timer, irr_database_t *db);
void irr_clean_timer (mtimer_t *timer, irr_database_t *db);
int irr_service_mirror_request (irr_connection_t *irr, char *command);
int request_mirror (irr_database_t *db, uii_connection_t *uii, uint32_t last);
int valid_start_line (irr_database_t *database, FILE *fp, uint32_t *serial_num);
int get_remote_mirrorstatus (prefix_t *mirror_prefix, int mirror_port);

/* commands.c */
void irr_process_command (irr_connection_t * irr);

/* scan */
char *scan_irr_file (irr_database_t *database, char *extension, 
		   int update_flag, FILE *fp);
void *scan_irr_file_main (FILE *fp, irr_database_t *database,
                                  int update_flag, enum SCAN_T scan_scope);
int write_irr_serial (irr_database_t *database);
void write_irr_serial_export (uint32_t serial, irr_database_t *database);
int scan_irr_serial (irr_database_t *database);
int get_state (char *buf, u_long len, enum STATES state, enum STATES *p_save_state);
int pick_off_mirror_hdr (FILE *fp, char *buf, int buf_size, 
                         enum STATES state, enum STATES *p_save_state,
			 u_long *mode, u_long *position,
			 u_long *offset, irr_database_t *db);
int get_curr_f (char *buf);

/* RPSL */
void irr_set_expand (irr_connection_t *irr, char *name);

/* indicies */
int irr_database_find_matches (irr_connection_t *irr, char *key, 
				   u_char p_or_s,
				   int match_behavior,
				   enum IRR_OBJECTS type,
                                   u_long *ret_offset, u_long *ret_len); 
int irr_database_store (irr_database_t *database, char *key, u_char p_or_s,
			enum IRR_OBJECTS type, u_long offset, u_long len);
int irr_database_remove (irr_database_t *database, char *key, u_long offset);
void make_spec_key (char *new_key, char *maint, char *set_name);
void make_mntobj_key (char *new_key, char *maint);
void make_6as_key (char *gas_key, char *origin);
void make_gas_key (char *gas_key, char *origin);
void make_setobj_key (char *new_key, char *obj_name);
int irr_hash_destroy (hash_item_t *hash_item);
void store_hash_spec (irr_database_t *database, hash_spec_t *hash_item);
hash_spec_t *fetch_hash_spec (irr_database_t *database, char *key,
                              enum FETCH_T mode); 
int find_object_offset_len (irr_database_t *db, char *key,
			enum IRR_OBJECTS type, u_long *offset, u_long *len);
void Delete_hash_spec (hash_spec_t *hash_item); 
void commit_spec_hash (irr_database_t *db);
int memory_hash_spec_remove (irr_database_t *db, char *key, enum SPEC_KEYS id,
				irr_object_t *object); 
int memory_hash_spec_store (irr_database_t *db, char *key, enum SPEC_KEYS id,
				irr_object_t *object);

/* routines handle prefixes */
void add_irr_prefix (irr_database_t *database, prefix_t *prefix, irr_object_t *object);
int delete_irr_prefix (irr_database_t *database, prefix_t *prefix, irr_object_t *object);
int seek_prefix_object (irr_database_t *database, enum IRR_OBJECTS type, char *key, 
		       uint32_t origin, u_long *offset, u_long *len);
radix_node_t *prefix_search_exact (irr_database_t *database, prefix_t *prefix);
radix_node_t *prefix_search_best (irr_database_t *database, prefix_t *prefix);

/* other */
void convert_toupper(char *p);

/* Statusfile related functions */
statusfile_t * InitStatusFile (char *filename);
int CloseStatusFile (statusfile_t *sf);
char * GetStatusString (statusfile_t *sf, char *section, char *variable);
int SetStatusString (statusfile_t *sf, char *section, char *variable, char *value);
void uii_show_statusfile (uii_connection_t *uii);
int config_statusfile (uii_connection_t *uii, char *filename);

/* atomic transaction support */

int     rollback_check      (rollback_t *);
int     reapply_transaction (rollback_t *);
char    *build_transaction_file (irr_database_t *, FILE *, char *, char *, 
				 int *);
int     db_rollback         (irr_database_t *, char *);
int     journal_rollback    (irr_database_t *, char *);
int     update_journal      (FILE *, irr_database_t *, int);

