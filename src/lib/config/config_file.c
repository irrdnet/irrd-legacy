/*
 * $Id: config_file.c,v 1.3 2002/02/04 20:31:50 ljb Exp $
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <mrt.h>
#include <irrdmem.h>
#include <config_file.h>

/* globals */
config_t CONFIG;

static char debug_config[255] = "No tracing enabled.";
static char encripted_password[] = "********";


static void
config_call_module (config_module_t * module)
{
#ifdef HAVE_LIBPTHREAD
    if (BIT_TEST (module->flags, CF_WAIT)) {
	pthread_mutex_lock (&CONFIG.mutex_cond_lock);
	module->call_fn (module->arg);
	pthread_cond_wait (&CONFIG.cond, &CONFIG.mutex_cond_lock);
	pthread_mutex_unlock (&CONFIG.mutex_cond_lock);
    } else
#endif	/* HAVE_LIBPTHREAD */
	module->call_fn (module->arg);
}


/*
 * config_add_module Modules store configuration file information as a linked
 * list of modules. Each module has a call_back function and argument to
 * display that line of the configuration file. Since some configuration
 * information may be stored in memory used by threads, we may need to
 * schedule the call_back routine to store data in the CONFIG.answer
 * location. The wait_flag argument specifies whether we need to wait for
 * another thread to fill in the data.
 */
int 
config_add_module (int flags, char *name, void_fn_t call_fn, void *arg)
{
    config_module_t *module;

    /*
     * need to make sure we don't pick up multiple times, i.e. for access
     * list
     */
    pthread_mutex_lock (&CONFIG.ll_modules_mutex_lock);
    LL_Iterate (CONFIG.ll_modules, module) {
	if (call_fn && (module->call_fn != call_fn))
	    continue;
	if (arg && (module->arg != arg))
	    continue;
        pthread_mutex_unlock (&CONFIG.ll_modules_mutex_lock);
	return (0);
    }

    module = (config_module_t*) irrd_malloc(sizeof(config_module_t));
    module->call_fn = call_fn;
    module->arg = arg;
    module->flags = flags;
    if (name)	/* I don't know that all calls supply the name -- masaki */
	module->name = strdup (name);

    LL_Add (CONFIG.ll_modules, module);
    pthread_mutex_unlock (&CONFIG.ll_modules_mutex_lock);
    return (1);
}


int 
config_del_module (int flags, char *name, void_fn_t call_fn, void *arg)
{
    config_module_t *module;

    pthread_mutex_lock (&CONFIG.ll_modules_mutex_lock);
    LL_Iterate (CONFIG.ll_modules, module) {
	if (module->name && name && strcmp (module->name, name) != 0)
	    continue;
	if (call_fn && (module->call_fn != call_fn))
	    continue;
	if (arg && (module->arg != arg))
	    continue;

	    /* XXX comment is never deleted */
	    if (strcmp (module->name, "!") == 0)
		irrd_free(module->arg);
	    if (module->name)
		irrd_free(module->name);
	    LL_Remove (CONFIG.ll_modules, module);
	    irrd_free(module);
    	    pthread_mutex_unlock (&CONFIG.ll_modules_mutex_lock);
	    return (1);
    }
    pthread_mutex_unlock (&CONFIG.ll_modules_mutex_lock);
    return (0);
}


/* this version allows multiple lines */
void 
get_comment_config (char *comment)
{
    char *cp, *cp2;
    int cc;

    cp = comment;
    if (cp == NULL || *cp == '\0') {
	return;
    }
    cc = *cp++;

    while ((cp2 = strchr (cp, '\n')) != NULL) {
	int save = cp2[1];
	cp2[1] = '\0';
	config_add_output ("%c%s", cc, cp);
	cp2[1] = save;
	cp = cp2 + 1;
    }
    config_add_output ("%c%s\n", cc, cp);
}


/* there is no way to delete comment */
static int 
config_comment (uii_connection_t * uii)
{
/* network list has a conflicting command "route-map" */
if (CONFIG.state_eof && uii->state == UII_CONFIG_NETWORK_LIST) {
    /*
     * if this is a blank line comment, assume we have dropped out of a
     * higher state, though it might not be to return to UII_CONFIG... hmm...
     * we need to remember immediate lower state (do we need a stack?) we
     * treat comments longer than one character as a regular comment and
     * don't change state
     */
    if (strlen (uii->cp) < 2) {
	trace (TR_PARSE, CONFIG.trace, "COMMENT -- STATE EOF\n");
	if (uii->prev_level >= 0 && uii->state > UII_CONFIG)
	    uii->state = uii->previous[uii->prev_level--];
    }
}

    config_add_module (0, "!", get_comment_config, strdup (uii->cp));
    return (1);
}


static void 
get_enable_password (void)
{
    char *password = encripted_password;

    assert (UII->enable_password);
    if (CONFIG.show_password)
	password = UII->enable_password;
    config_add_output ("enable password %s\n", password);
}     


static int
config_enable_password (uii_connection_t * uii, char *password)
{
    char *cp;

    if (uii->negative) {
	set_uii (UII, UII_ENABLE_PASSWORD, NULL, 0);
        config_del_module (0, "enable password", NULL, NULL);
	return (1);
    }
    cp = strip_spaces (password);
    set_uii (UII, UII_ENABLE_PASSWORD, cp, 0);
    config_add_module (0, "enable password", get_enable_password, NULL);
    irrd_free(password);
    return (1);
}


static int
enable (uii_connection_t * uii)
{
    if (uii->state == UII_ENABLE)
	return (0);
    assert (uii->state == UII_NORMAL);
    if (UII->enable_password == NULL) {
	uii->state = UII_ENABLE;
    } else {
	uii->previous[++uii->prev_level] = uii->state;
	uii->state = UII_UNPREV;
    }
    return (1);
}


static void 
get_uii_redirect (void)
{
    config_add_output ("redirect %s\n", UII->redirect);
} 


/*
 * config_redirect redirect %s Allow redirection to files in this directory
 */
static int 
config_redirect (uii_connection_t * uii, char *directory)
{
    if (UII->redirect)
	irrd_free(UII->redirect);

    if (uii->negative) {
        UII->redirect = NULL;
        config_del_module (0, "redirect", NULL, NULL);
	return (1);
    }
    UII->redirect = directory;
    config_add_module (0, "redirect", get_uii_redirect, NULL);
    return (1);
}


int
get_alist_options (char *options, prefix_t ** wildcard, int *refine, int *exact)
{
    if (options) {
	char *line = options, *opt;
	prefix_t *pp;

	/* this allows more than one occurence of the same option */
	while ((opt = uii_parse_line (&line)) != NULL) {
	    if (strcasecmp (opt, "refine") == 0) {
		*refine = 1;
	    } else if (strcasecmp (opt, "exact") == 0) {
		*exact = 1;
	    } else if ((pp = ascii2prefix (0, opt)) != NULL) {
		if (*wildcard)
		    Deref_Prefix (*wildcard);
		*wildcard = pp;
	    } else {
		if (*wildcard)
		    Deref_Prefix (*wildcard);
		return (-1);
	    }
	}
	return (0);
    }
    return (0);
}


static void 
get_access_list_config (int num)
{
    access_list_out (num, (void_fn_t) config_add_output);
}


/*
 * config_access_list
 */
static int
config_access_list (uii_connection_t * uii, int num,
		    char *permit_or_deny, prefix_t * prefix, char *options)
{
    int permit, refine = 0, exact = 0;
    prefix_t *wildcard = NULL;
    char tmpx[BUFSIZE];
    int n;

    permit = (strcasecmp (permit_or_deny, "permit") == 0);
    irrd_free(permit_or_deny);

    if (options) {
	get_alist_options (options, &wildcard, &refine, &exact);
	irrd_free(options);
    }
    if (num <= 0 && num >= MAX_ALIST) {
	config_notice (TR_ERROR, uii,
		       "CONFIG invalid access-list number %d\n", num);
	return (-1);
    }
    if (prefix) {
	if (wildcard) {
	    sprintf (tmpx, "%s %s", prefix_toax (prefix),
		     prefix_toa (wildcard));
	} else {
	    sprintf (tmpx, "%s", prefix_toax (prefix));
	}
	if (exact)
	    strcat (tmpx, " exact");
	if (refine)
	    strcat (tmpx, " refine");
    } else {
	sprintf (tmpx, "all");
    }

    if (uii->negative) {
        n = remove_access_list (num, permit, prefix, wildcard, exact, refine);
	if (n == 0) {
            config_del_module (0, "access-list", get_access_list_config, 
			       (void*) &num);
	}
    	return (1);
    }

    n = add_access_list (num, permit, prefix, wildcard, exact, refine);
    if (n <= 0) {
	config_notice (TR_ERROR, uii,
		       "CONFIG: Error -- access list not added to %d\n", num);
	return (-1);
    }
    if (n == 1)
        config_add_module (0, "access-list", get_access_list_config, 
			   (void *) &num);
    return (1);
}


static int
config_access_list_all (uii_connection_t * uii, int num,
			char *permit_or_deny)
{
    return (config_access_list (uii, num, permit_or_deny, NULL, NULL));
}


/*
 * parse_debug debug (norm|trace) %s [%d] Set GLOBAL trace values
 */
static int 
parse_debug (uii_connection_t * uii, trace_t * tr, char *s)
{
    char *token, *line;
    u_long flag = 0;

    line = s;

    /* get flag */
    if ((token = (char *) uii_parse_line (&line)) == NULL) {
	uii_send_data (uii,
		       "Usage: debug trace_flag [file] [max_filesize]\n");
	return (-1);
    }
    flag = trace_flag (token);
    if (flag == 0) {
	uii_send_data (uii,
		       "Usage: debug trace_flag [file] [max_filesize]\n");
	return (-1);
    }
    trace (NORM, CONFIG.trace, "CONFIG debug %s\n", line);

    sprintf (debug_config, "Tracing %s output", token);

    set_trace (tr, TRACE_ADD_FLAGS, flag, NULL);

    /* get file name (OPTIONAL) */
    if ((token = (char *) uii_parse_line (&line)) == NULL) {
	strcat (debug_config, " to default MRT trace file /tmp/mrt.log");
	return (1);
    }
    set_trace (tr, TRACE_LOGFILE, token, NULL);
    strcat (debug_config, " to ");
    strcat (debug_config, token);

    /* get max. trace filesize (OPTIONAL) */
    if ((token = (char *) uii_parse_line (&line)) == NULL)
	return (1);

    set_trace (tr, TRACE_MAX_FILESIZE, atoi (token), NULL);
    strcat (debug_config, ", truncating log at ");
    strcat (debug_config, token);
    strcat (debug_config, " bytes");

    /* making global changes -- change all other trace routines */
    set_trace_global (tr);

    return (1);
}


void 
get_debug_config (void)
{
    int first = 1;

    if (BIT_TEST (CONFIG.trace->flags, TR_ALL)) {
	first = 0;
	config_add_output ("debug all ");
    } else {
	if (BIT_TEST (CONFIG.trace->flags, TR_INFO)) {
	    if (first != 1)
		config_add_output ("\n");
	    config_add_output ("debug info ");
	}
	if (BIT_TEST (CONFIG.trace->flags, NORM)) {
	    if (first != 1)
		config_add_output ("\n");
	    config_add_output ("debug norm ");
	}
	if (BIT_TEST (CONFIG.trace->flags, TRACE)) {
	    if (first != 1)
		config_add_output ("\n");
	    config_add_output ("debug trace ");
	}
	if (BIT_TEST (CONFIG.trace->flags, TR_PARSE)) {
	    if (first != 1)
		config_add_output ("\n");
	    config_add_output ("debug trace ");
	}
	if (BIT_TEST (CONFIG.trace->flags, TR_PACKET)) {
	    if (first != 1)
		config_add_output ("\n");
	    config_add_output ("debug packet ");
	}
	if (BIT_TEST (CONFIG.trace->flags, TR_STATE)) {
	    if (first != 1)
		config_add_output ("\n");
	    config_add_output ("debug state ");
	}
    }

    if (first == 1)
	return;			/* debugging is not turned on.... */

    config_add_output ("%s ", CONFIG.trace->logfile->logfile_name);

    if (!strcmp ("stdout", CONFIG.trace->logfile->logfile_name))
	config_add_output ("\n");
    else
	config_add_output ("%d\n", CONFIG.trace->logfile->max_filesize);

    return;
}


static int 
config_debug (uii_connection_t * uii, char *s)
{
    parse_debug (uii, CONFIG.trace, s);
    config_add_module (0, "debug", get_debug_config, NULL);
    irrd_free(s);
    return (1);
}


#if 0
void 
wait_config_data (void_fn_t call_fn, void *arg)
{
    char *tmp;

#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock (&CONFIG.mutex_cond_lock);
#endif				/* HAVE_LIBPTHREAD */

    call_fn (arg);

#ifdef HAVE_LIBPTHREAD
    pthread_cond_wait (&CONFIG.cond, &CONFIG.mutex_cond_lock);
    pthread_mutex_unlock (&CONFIG.mutex_cond_lock);
#endif				/* HAVE_LIBPTHREAD */

    return;
}
#endif


#ifdef notdef
/* Okay, signal the waiting uii that data is ready for it */
static void 
signal_config_data_ready ()
{
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock (&CONFIG.mutex_cond_lock);
    pthread_cond_signal (&CONFIG.cond);
    pthread_mutex_unlock (&CONFIG.mutex_cond_lock);
#endif				/* HAVE_LIBPTHREAD */
}
#endif


void 
config_add_output (char *format, ...)
{
    va_list args;

    if (CONFIG.answer == NULL)
	CONFIG.answer = New_Buffer (0);

    va_start (args, format);
    buffer_vprintf (CONFIG.answer, format, args);
    va_end (args);
}


/*
 * config_notice Convenience function to send error message both to log file
 * and out to uii
 */
void 
config_notice (int trace_flag, uii_connection_t * uii, char *format, ...)
{
    va_list args;

    va_start (args, format);
    user_vnotice (trace_flag, CONFIG.trace, uii, format, args);
    va_end (args);
}


static void
config_check_delimiter (config_module_t *module)
{
	/* we are a module like router_bgp and need a "!" delimiter */
	if (BIT_TEST (module->flags, CF_DELIM)) {
	    config_module_t *tmp_module;
	    tmp_module = LL_GetNext (CONFIG.ll_modules, module);
	    if (tmp_module != NULL && 
		    strcmp (tmp_module->name, "!") != 0) {
		config_add_output ("!\n");
	    }
	}
}


int 
show_config (uii_connection_t * uii)
{
    config_module_t *module;

    pthread_mutex_lock (&CONFIG.ll_modules_mutex_lock);

    LL_Iterate (CONFIG.ll_modules, module) {

	/*
	 * Why wasn't uii passed to config_call_module to be used in
	 * printing? If it were so, I think that show_config could be
	 * re-entrant. CONFIG.answer is shared, so I need lock/unlock here --
	 * masaki
	 */

	config_call_module (module);
	config_check_delimiter (module);
    }

	//if (CONFIG.answer == NULL) {
	//	return;
	//}

    if (buffer_data_len (CONFIG.answer) > 0) {
		uii_add_bulk_output (uii, "%s", buffer_data (CONFIG.answer));
		buffer_reset (CONFIG.answer);
    }
    pthread_mutex_unlock (&CONFIG.ll_modules_mutex_lock);
    return (1);
}


int 
config_write (uii_connection_t * uii)
{
    config_module_t *module;
    char *filename, *cp, *data, *end;
    FILE *fd;

    /*
     * we probably need to freeze config changes when we do this... i.e. it
     * would be bad if someone else was hacking the config file
     */

    if ((filename = CONFIG.filename) == NULL) {
	return (-1);
    }

    if (access (filename, F_OK) == 0) {
	uii_send_data (uii, "Overwrite to %s (yes/no)? ", filename);
	if (!uii_yes_no (uii))
	    return (0);
    }
    if ((fd = fopen (filename, "w")) == NULL) {
	config_notice (NORM, uii,
	       "Write to config file %s failed (%m)\n", filename);
	return (-1);
    }
    config_notice (NORM, uii,
	"Writing configuration file to %s\n", filename);

    pthread_mutex_lock (&CONFIG.ll_modules_mutex_lock);
    CONFIG.show_password++;
    LL_Iterate (CONFIG.ll_modules, module) {
	config_call_module (module);
	config_check_delimiter (module);
    }
    CONFIG.show_password = 0;
    pthread_mutex_unlock (&CONFIG.ll_modules_mutex_lock);

    data = cp = (char *) buffer_data (CONFIG.answer);
    end = data + buffer_data_len (CONFIG.answer);
    while (cp < end) {
	if (*cp != '\r')
	    putc (*cp, fd);
	 cp++;
    }
    buffer_reset (CONFIG.answer);
    fclose (fd);
    return (1);
}


/*
 * start_config call by uii thread.
 */
int 
start_config (uii_connection_t * uii)
{
    if (pthread_mutex_trylock (&CONFIG.mutex_lock) != 0) {
	config_notice (TR_INFO, uii, "CONFIG mode is already locked\n");
	return (-1);
    }

    /* uii functions are shared among programs that may not have config mode */
    /* so it has to pass the lock in order to unlock it on error close */
    uii->mutex_lock_p = &CONFIG.mutex_lock;
    uii->previous[++uii->prev_level] = uii->state;
    uii->state = UII_CONFIG;
    return (0);
}


void 
end_config (uii_connection_t * uii)
{
    assert (uii->mutex_lock_p);
    pthread_mutex_unlock (uii->mutex_lock_p);
}


static void
get_config_line_vty (void)
{
    config_add_output ("line vty\n");
    if (UII->login_enabled)
        config_add_output ("  login\n");
    if (UII->password) {
	char *password = encripted_password;
	if (CONFIG.show_password)
	    password = UII->password;
        config_add_output ("  password %s\n", password);
    }
    if (UII->access_list >= 0)
        config_add_output ("  access-class %d\n", UII->access_list);
    if (CONFIG.timeout_min >= 0 && CONFIG.timeout_sec < 0)
        config_add_output ("  exec-timeout %d\n", CONFIG.timeout_min);
    else if (CONFIG.timeout_min >= 0 && CONFIG.timeout_sec >= 0)
        config_add_output ("  exec-timeout %d %d\n", CONFIG.timeout_min,
						     CONFIG.timeout_sec);
    if (CONFIG.port >= 0)
        config_add_output ("  port %d\n", CONFIG.port);
    if (UII->prefix != NULL)
        config_add_output ("  address %p\n", UII->prefix);
}


static int
config_line_vty (uii_connection_t * uii)
{
    if (uii->negative) {
	config_del_module (0, "line vty", NULL, NULL);
	return (1);
    }
    uii->previous[++uii->prev_level] = uii->state;
    uii->state = UII_CONFIG_LINE;
    config_add_module (CF_DELIM, "line vty", get_config_line_vty, NULL);
    return (1);
}


static int
config_line_login (uii_connection_t * uii)
{
    if (uii->negative) {
        UII->login_enabled = 0;
        return (1);
    }
    UII->login_enabled = 1;
    return (1);
}


static int 
config_line_password (uii_connection_t * uii, char *password)
{
    char *cp;

    if (uii->negative) {
        set_uii (UII, UII_PASSWORD, NULL, 0);
        return (1);
    }
    cp = strip_spaces (password);
    set_uii (UII, UII_PASSWORD, cp, 0);
    irrd_free(password);
    return (1);
}


static int 
config_line_access_class (uii_connection_t * uii, int num)
{
    if (uii->negative) {
        set_uii (UII, UII_ACCESS_LIST, num, 0);
        return (1);
    }

    set_uii (UII, UII_ACCESS_LIST, num, 0);
    return (1);
}


static int 
config_line_timeout (uii_connection_t * uii, int min, int opt, int sec)
{
    if (uii->negative) {
        UII->timeout = UII_DEFAULT_TIMEOUT;
        CONFIG.timeout_min = -1;
        CONFIG.timeout_sec = -1;
        return (1);
    }

    CONFIG.timeout_min = min;
    if (opt > 0) {
        CONFIG.timeout_sec = sec;
        UII->timeout = min * 60 + sec;
    }
    else {
        CONFIG.timeout_sec = -1;
        UII->timeout = min * 60;
    }
    return (1);
}


static int
config_line_port (uii_connection_t * uii, int port)
{
    if (uii->negative) {
	if (UII->port != UII->default_port) {
	    assert (UII->default_port >= 0);
    	    set_uii (UII, UII_PORT, UII->default_port, 0);
	    if (!CONFIG.reading_from_file)
	        listen_uii2 (NULL);
	}
        return (1);
    }
    CONFIG.port = port;
    if (UII->port != port) {
    	set_uii (UII, UII_PORT, port, 0);
	if (!CONFIG.reading_from_file)
	    listen_uii2 (NULL);
    }
    return (1);
}


static int
config_line_address (uii_connection_t * uii, prefix_t *prefix)
{
    if (uii->negative) {
	if (UII->prefix != NULL) {
    	    set_uii (UII, UII_ADDR, NULL, 0);
	    listen_uii2 (NULL);
	}
	Deref_Prefix (prefix);
        return (1);
    }
    if (prefix_compare2 (UII->prefix, prefix) != 0) {
    	set_uii (UII, UII_ADDR, prefix, 0);
	listen_uii2 (NULL);
    }
    Deref_Prefix (prefix);
    return (1);
}


static int 
config_uii_port (uii_connection_t * uii, int port)
{
    config_add_module (CF_DELIM, "line vty", get_config_line_vty, NULL);
    return (config_line_port (uii, port));
}


static int 
config_uii_addr (uii_connection_t * uii, prefix_t * prefix)
{
    config_add_module (CF_DELIM, "line vty", get_config_line_vty, NULL);
    return (config_line_address (uii, prefix));
}


static int 
config_uii_timeout (uii_connection_t * uii, int timeout)
{
    config_add_module (CF_DELIM, "line vty", get_config_line_vty, NULL);
    return (config_line_timeout (uii, 0, 1, timeout));
}


/*
 * config_password config password %s [%d]
 */
static int 
config_password (uii_connection_t * uii, char *password, int num_optional,
		 int access_list)
{
    if (!uii->negative) {
        config_add_module (CF_DELIM, "line vty", get_config_line_vty, NULL);
    }
    config_line_password (uii, password); /* free password */
    /* this is previous semantics. if password set, login enabled */
    if (uii->negative)
	UII->login_enabled = 0;
    else
	UII->login_enabled++;
    if (num_optional > 0)
        config_line_access_class (uii, access_list);
    return (1);
}


int 
init_config (trace_t * tr)
{
    CONFIG.ll_modules = LL_Create (0);
    pthread_mutex_init (&CONFIG.ll_modules_mutex_lock, NULL);
    pthread_cond_init (&CONFIG.cond, NULL);
    pthread_mutex_init (&CONFIG.mutex_cond_lock, NULL);
    pthread_mutex_init (&CONFIG.mutex_lock, NULL);
    CONFIG.trace = tr;
    CONFIG.timeout_min = -1;
    CONFIG.timeout_sec = -1;
    CONFIG.port = -1;

    uii_add_command2 (UII_NORMAL, 0, "enable", enable, "Enable");

    /* Config mode */
    uii_add_command2 (UII_ALL, 0, "show config",
                      show_config, "Display current configuration");
    uii_add_command2 (UII_ENABLE, 0, "config",
                      start_config, "Configure MRTd");
    uii_add_command2 (UII_ENABLE, 0, "write", config_write,
                      "Save configuration file to disk");

    /* config top level */
    uii_add_command2 (UII_CONFIG, 0, "line vty",
		      config_line_vty, "Enable telnet interface");
    uii_add_command2 (UII_CONFIG, 0, "no line vty",
		      config_line_vty, "Disable telnet interface");
    uii_add_command2 (UII_CONFIG_LINE, 0, "login",
		      config_line_login, "Enable login");
    uii_add_command2 (UII_CONFIG_LINE, 0, "no login",
		      config_line_login, "Disable login");
    uii_add_command2 (UII_CONFIG_LINE, 0, "password %s",
		      config_line_password,
		      "Set telnet interface password");
    uii_add_command2 (UII_CONFIG_LINE, 0, "no password",
		      config_line_password, "Delete password");
    uii_add_command2 (UII_CONFIG_LINE, 0, "access-class %d",
		      config_line_access_class, "Define access list");
    uii_add_command2 (UII_CONFIG_LINE, 0, "no access-class",
		      config_line_access_class, "Delete access list");
    uii_add_command2 (UII_CONFIG_LINE, 0, "exec-timeout %dMinutes [%dSeconds]",
		      config_line_timeout, "Set timeout");
    uii_add_command2 (UII_CONFIG_LINE, 0, "no exec-timeout",
		      config_line_timeout, "Set default to timeout");
    uii_add_command2 (UII_CONFIG_LINE, 0, "port %d",
		      config_line_port, "Specify port number");
    uii_add_command2 (UII_CONFIG_LINE, 0, "no port",
		      config_line_port, "Reset port number to default");
    uii_add_command2 (UII_CONFIG_LINE, 0, "address %M",
		      config_line_address, "Specify binding address");
    uii_add_command2 (UII_CONFIG_LINE, 0, "no address",
		      config_line_address, "Free binding address");

    uii_add_command2 (UII_CONFIG, 0, "password %s [%d]",
		      config_password,
		      "Set telnet interface password");
    uii_add_command2 (UII_CONFIG, 0, "no password",
		      config_password, "Delete password");
    uii_add_command2 (UII_CONFIG, 0, "enable password %s",
		      config_enable_password,
		      "Set enable password");
    uii_add_command2 (UII_CONFIG, 0, "no enable password",
		      config_enable_password,
		      "Delete enable password");

    uii_add_command2 (UII_CONFIG, 0, "uii_port %d", config_uii_port,
		      "Set telnet interface port number");
    uii_add_command2 (UII_CONFIG, 0, "no uii_port", 
		      config_uii_port,
		      "Disable telnet interface port number");
    uii_add_command2 (UII_CONFIG, 0, "uii_addr %M", config_uii_addr,
		      "Set telnet interface address");
    uii_add_command2 (UII_CONFIG, 0, "no uii_addr", config_uii_addr,
		      "Delete telnet interface address");
    uii_add_command2 (UII_CONFIG, 0, "uii_timeout %d", config_uii_timeout,
		      "Set UII timeout in sec");
    uii_add_command2 (UII_CONFIG, 0, "no uii_timeout", config_uii_timeout,
		      "Delete UII timeout");

    uii_add_command2 (UII_CONFIG, 0, "debug %S", config_debug,
		      "Configure logging information");
    uii_add_command2 (UII_CONFIG, 0,
		      "access-list %d (permit|deny) %m %S",
		      config_access_list, "Add IP based acl lists");
    uii_add_command2 (UII_CONFIG, 0,
		      "access-list %d (permit|deny) all",
		      config_access_list_all, "Add IP based acl lists");
    uii_add_command2 (UII_CONFIG, 0,
		      "no access-list %d (permit|deny) %m %S",
		      config_access_list, "Delete IP based acl lists");
    uii_add_command2 (UII_CONFIG, 0,
		      "no access-list %d (permit|deny) all",
		      config_access_list_all, "Delete IP based acl lists");

    uii_add_command2 (UII_CONFIG, 0, "redirect %s", 
		      config_redirect, "Set redirect directory");
    uii_add_command2 (UII_CONFIG, 0, "no redirect", 
		      config_redirect, "Disable redirection");

    uii_add_command2 (UII_CONFIG_ALL, COMMAND_NODISPLAY | COMMAND_MATCH_FIRST,
		      "#", config_comment, NULL);
    uii_add_command2 (UII_CONFIG_ALL, COMMAND_NODISPLAY | COMMAND_MATCH_FIRST,
		      "!", config_comment, NULL);

    return (1);
}


/*
 * config_file_file a backwards compatibility kludge
 */
int 
config_from_file (trace_t * tr, char *filename)
{
    init_config (tr);
    return (config_from_file2 (tr, filename));
}


/*
 * config_from_file2
 */
int 
config_from_file2 (trace_t * tr, char *filename)
{
    uii_connection_t *uii;
    int errors = 0;
    FILE *fd;

    if (filename == NULL)
	return (-1);

    MRT->config_file_name = strdup (filename);
    CONFIG.filename = strdup (filename);
    uii = (uii_connection_t*) irrd_malloc(sizeof(uii_connection_t));
    uii->sockfd = -1;
    uii->sockfd_out = -1;
    uii->mutex_lock_p = NULL;

    if ((filename == NULL) || ((fd = fopen (filename, "r")) == NULL)) {
	trace (NORM, CONFIG.trace, "Failed to open config file %s\n", filename);
	trace (NORM, CONFIG.trace, "Using default values only.\n");
	irrd_free(uii);
	return (-1);
    }

    start_config (uii);
    CONFIG.line = 0;
    CONFIG.reading_from_file = 1;

    uii->buffer = New_Buffer_Stream (fd);
    while (buffer_gets (uii->buffer) > 0) {
	CONFIG.line++;
	if (uii_process_command (uii) < 0) {
	    errors++;
	    trace (ERROR, CONFIG.trace, 
		"CONFIG ERROR at line %d\n", CONFIG.line);
	    trace (ERROR, CONFIG.trace, "\t%s\n", buffer_data (uii->buffer));
	    if (!CONFIG.ignore_errors)
		exit (0);
	}
    }

    CONFIG.reading_from_file = 0;
    fclose (fd);
    Delete_Buffer (uii->buffer);
    if (uii->ll_tokens != NULL)
	LL_Destroy (uii->ll_tokens);
    end_config (uii);
    irrd_free(uii);

    /* don't use fatal for now while developing code... just ignore */
    if (errors) {
	trace (NORM, CONFIG.trace, "%d CONFIG ERROR(s)\n", errors);
    }
    return (1);
}

void 
config_create_default (void)
{
    char strbuf[BUFSIZE];

    config_add_module (0, "!", get_comment_config, strdup (
    "#####################################################################"));

    sprintf (strbuf, "# MRTd -- MRT version %s ", MRT->version);
    config_add_module (0, "!", get_comment_config, strdup (strbuf));

    config_add_module (0, "!", get_comment_config, strdup (
    "#####################################################################"));

    config_add_module (0, "!", get_comment_config, strdup ("#"));
    config_add_module (0, "debug", get_debug_config, NULL);
    config_add_module (0, "!", get_comment_config, strdup ("#"));

#ifdef notdef
    set_uii (UII, UII_PASSWORD, strdup ("foo"), 0);
    config_add_module (0, "password", get_uii_password, NULL);
    set_uii (UII, UII_ENABLE_PASSWORD, strdup ("foo"), 0);
    config_add_module (0, "enable password", get_enable_password, NULL);
    set_uii (UII, UII_PORT, UII_DEFAULT_PORT, 0);
    config_add_module (0, "uii_port", get_config_uii_port, NULL);
#endif
}
