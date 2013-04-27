#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#ifndef NT
#include <dirent.h>
#endif /* NT */
#include <errno.h>

#include <atomic_ops.h>
#include <pipeline_defs.h>

/* Solaris 2.5.1 lacks prototype for utimes() */
#if defined(HAVE_DECL_UTIMES) && ! HAVE_DECL_UTIMES
extern int utimes(char *file, struct timeval *tvp);
#endif

/* lokel yokel's */

static int  fastcopy	   (char *from, char *to, int domv);
static int  my_mv          (char *, char *);
static int  my_cp          (char *, char *);
static int  file_exists    (char *, char *);
static int  restore_files  (uii_connection_t *, atom_finfo_t *);
static int  my_rename      (char *, char *, uii_connection_t *);
static int  rm_tmpfiles    (atom_finfo_t *);
static void rm_backups     (atom_finfo_t *); 

/* Move file (name) to directory (dir).  
 *
 * First a backup copy of (name) is made and then a move
 * operation is performed from a temp dir to (dir).  The 
 * temp dir is defined in the (af) struct.  If any errors 
 * occur the backup copy is restored and the operation is aborted. 
 *
 * This function can backout from any point in the process
 * of moving multiple files into (dir) (eg, RGNET.CURRENTSERIAL
 * and rgnet.db).  It keeps a list of files in (af) indicating
 * which files need to be restored in case of an abort operation.
 *
 * Input:
 *  -target directory for the move operation (dir)
 *  -name of the file to move (name)
 *  -struct to allow messages to be passed to a UII user (uii)
 *  -struct which defines the from dir of the move and also the list 
 *   of files which would need to be restored in case of an abort 
 *   operation (af)
 *
 * Return:
 *  -1 if the operation was a success
 *  -0 otherwise, in which case the old file(s) state have been restored
 */
int atomic_move (char *dir, char *name, uii_connection_t *uii, 
		 atom_finfo_t *af) {
  int made_backup = 1;
  char dbname[BUFSIZE+1], tname[BUFSIZE+1];

  /* sanity check */
  if (dir  == NULL ||
      name == NULL ||
      af   == NULL) {
    trace (ERROR, default_trace, 
	   "atomic_move (): NULL dir (%s), name (%s) or af (%s).\n",
	   ((dir == NULL) ? "NULL" : dir), ((name == NULL) ? "NULL" : name),
	   ((af == NULL) ? "NULL" : "non-NULL"));
    return 0;
  }

  /* target path for the move */
  sprintf (dbname, "%s/%s", dir, name);
  
  /* trace (NORM, default_trace, "atomic_move: dbname (%s)\n", dbname); */

  /* do we need to make a backup copy?  no if no orignal exists */
  switch (file_exists (dir, name)) {
  case 1:  /* make a backup copy of (name) */
    sprintf (tname, "%s/%s.%s", dir, name, BAK_SFX);
    if (!my_rename (dbname, tname, uii)) {
      if (!restore_files (uii, af) ||
	  !rm_tmpfiles (af))
	exit (0);
      return 0;
    }
    break;
  case 0:  /* no file to make a backup from */
    made_backup = 0;
    break;
  default: /* couldn't read the cache directory */
    trace (ERROR, default_trace,
	   "atomic_move (): Insufficient permissions or bad directory path.  Backout name (%s) dir (%s)\n", name, dir);
    if (!restore_files (uii, af) ||
	!rm_tmpfiles (af))
      exit (0);
    return 0;
    break;
  }
  
  /* compile a list of files in case we need to restore state/files.
   * the restore function needs to know if a file does not exist 
   * and thus no backup was made.  see my_backout () for a description
   * of the string format of (file_list) */
  sprintf (&(af->file_list[strlen (af->file_list)]), "%c%s/%s!", 
	   ((made_backup) ? 'y' : 'n'), dir, name);
  sprintf (&(af->tmp_list[strlen (af->tmp_list)]), "%s!", name);

  /* trace (NORM, default_trace, "file_list (%s)\n", af->file_list);
     trace (NORM, default_trace, "tmp_list  (%s)\n", af->tmp_list); */

  /* move the new file from the temp dir to our cache area */
  sprintf (tname, "%s/%s", af->tmp_dir, name);
  if (!my_mv (tname, dbname)) {
    if (!restore_files (uii, af) ||
	!rm_tmpfiles (af))
      exit (0);

    return 0;
  }

  return 1;
}


/* Remove file (dir)/(name).  
 *
 * atomic_del () can be called multiple times to delete
 * a set of files atomically.  atomic_del () makes a 
 * backup of the file to be deleted and keeps track 
 * in the (af) struct.  If any of the files cannot be
 * deleted then all files are restored to their orignal state.
 *
 * Input:
 *  -directory of the file to be removed (dir)
 *  -name of the file to be removed (name)
 *  -struct to allow messages to be passed to a UII user (uii)
 *  -struct which defines the from dir of the move and also the list 
 *   of files which would need to be restored in case of an abort 
 *   operation (af)
 *
 * Return:
 *  -1 if the operation was a success
 *  -0 otherwise, in which case the previously deleted file(s) 
 *     have been restored
 */
int atomic_del (char *dir, char *name, uii_connection_t *uii, atom_finfo_t *af) {
  char fname[BUFSIZE+1], tname[BUFSIZE+1];

  /* do we need to make a backup copy?  no if no orignal exists */
  switch (file_exists (dir, name)) {
  case 1:  /* make a backup copy of (name) */
    sprintf (fname, "%s/%s", dir, name);
    sprintf (tname, "%s/%s.%s", dir, name, BAK_SFX);
    if (!my_cp (fname, tname)) {
      if (!restore_files (uii, af) ||
	  !rm_tmpfiles (af))
	exit (0);
      return 0;
    }
    break;
  case 0:  /* no file to make a backup, we're done */
    return 1;
    break;
  default: /* couldn't read the cache directory */
    trace (ERROR, default_trace,
	   "atomic_move (): Insufficient permissions or bad directory path.  Backout name (%s)\n", name);
    if (!restore_files (uii, af) ||
	!rm_tmpfiles (af))
      exit (0);
    return 0;
    break;
  }
  
  /* compile a list of files in case we need to restore state/files.
   * the restore function needs to know if a file does not exist 
   * and thus no backup was made.  see my_backout () for a description
   * of the string format of (file_list) */
  sprintf (&(af->file_list[strlen (af->file_list)]), "%c%s/%s!", 'y', dir, name);

  /* remove (fname) from our cache area */
  if (remove (fname)) {
    if (!restore_files (uii, af) ||
	!rm_tmpfiles (af))
      exit (0);

    return 0;
  }

  return 1;
}


/* Clean up the back-up files and temp ftp files after
 * an atomic operation.
 *
 * Input:
 *  -pointer to struct with back-up and temp file info (af)
 *
 * Return:
 *  -void
 *
 * Later there will be more to do to prepare for disk crashes
 */
void atomic_cleanup (atom_finfo_t *af) {
  rm_backups  (af);
  rm_tmpfiles (af);
}



/* Rename file (from) to (to).
 *
 * Files (from) and (to) need to be on the same filesystem
 *
 * Input:
 *  -old file name (from)
 *  -new file name (to)
 *  -uii connection for communication purposes (uii)
 *   ie, the user invoked a command from the uii interface
 *
 * Return:
 *  -1 if there were no errors
 *  -0 otherwise
 */
int my_rename (char *from, char *to, uii_connection_t *uii) {

  if (rename (from, to)) {
    trace (ERROR, default_trace, 
	   "my_rename () Unable to rename %s to %s:(%s)\n", 
	   from, to, strerror (errno));
    if (uii != NULL)
      uii_send_data (uii, "unable to rename (%s):(%s)\n", from, strerror (errno));
    
    return 0;
  }

  return 1;
}

#define FILEBUFSIZE 8096

/* a file copy function */
int fastcopy(char *from, char *to, int domv) {
  struct stat sb;
  struct timeval tval[2];
  char buff[FILEBUFSIZE];
  mode_t oldmode;
  register int nread, from_fd, to_fd;

  if (lstat(from, &sb)) {
    trace (ERROR, default_trace, 
	   "fastcopy () Unable to lstat %s:(%s)\n", 
	   from, strerror (errno));
    return (0);
  }

  if ((from_fd = open(from, O_RDONLY, 0)) < 0) {
    trace (ERROR, default_trace, 
	   "fastcopy () unable to open %s:(%s)\n", 
	   from, strerror (errno));
    return (0);
  }

  while ((to_fd =
    open(to, O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, 0)) < 0) {
      if (errno == EEXIST && unlink(to) == 0)
        continue;
      trace (ERROR, default_trace,
           "fastcopy () unable to open %s:(%s)\n",
            to, strerror (errno));
      (void)close(from_fd);
      return (0);
  }
  while ((nread = read(from_fd, buff, FILEBUFSIZE)) > 0)
    if (write(to_fd, buff, nread) != nread) {
      trace (ERROR, default_trace,
           "fastcopy () error writing %s:(%s)\n",
            to, strerror (errno));
      goto err;
    }

  if (nread < 0) {
      trace (ERROR, default_trace,
           "fastcopy () error reading %s:(%s)\n",
            from, strerror (errno));
err:  if (unlink(to))
        trace (ERROR, default_trace,
           "fastcopy () error unlinking %s:(%s)\n",
            to, strerror (errno));
      (void)close(from_fd);
      (void)close(to_fd);
      return (0);
  }
  (void)close(from_fd);

  oldmode = sb.st_mode;
  if (fchown(to_fd, sb.st_uid, sb.st_gid)) {
    if (oldmode & (S_ISUID | S_ISGID)) {
      sb.st_mode &= ~(S_ISUID | S_ISGID);
    }
  }
  if (fchmod(to_fd, sb.st_mode))
    trace (ERROR, default_trace,
      "fastcopy () fchmod %s set mode (was: )%03o :(%s)\n",
       to, oldmode, strerror (errno));

  if (domv) {
    tval[0].tv_sec = sb.st_atime;
    tval[1].tv_sec = sb.st_mtime;
    tval[0].tv_usec = tval[1].tv_usec = 0;
    if (utimes(to, tval))
      trace (ERROR, default_trace,
        "fastcopy () set times %s:(%s)\n",
         to, strerror (errno));
  }

  if (close(to_fd)) {
     trace (ERROR, default_trace,
        "fastcopy () error closing %s:(%s)\n",
         to, strerror (errno));
    return (0);
  }
  return (1);
}

/* Move file (from) to file (to).
 *
 * This function duplicates exactly the normal "mv".
 *
 * Input:
 *  -a fully qualified file name to be moved from (from)
 *  -a fully qualified file name to be move to (to)
 *
 * Return:
 *  -1 if there were no errors
 *  -0 otherwise
 */ 
int my_mv (char *from, char *to) {

  trace (NORM, default_trace, "my_mv from: %s  to: %s\n", from, to);

  /* try a rename first */
  if (!rename(from, to))
    return(1);

  /* if not trying to cross devices, return an error */
  if (errno != EXDEV) {
    trace (ERROR, default_trace, 
	   "my_mv () Unable to move %s to %s:(%s)\n", 
	   from, to, strerror (errno));
    return(0);
  }

  /* going across devices, need to do a copy */
  if (!fastcopy(from, to, 1))
    return(0);

  if (unlink(from)) {
    trace (ERROR, default_trace, 
	   "my_mv () Unable to unlink %s:(%s)\n", 
	   from, strerror (errno));
    return(0);
  }

  return(1);
}

/* Copy file (from) to file (to).
 *
 * This function duplicates exactly the normal "cp".
 *
 * It is expected this routine will be used to make
 * backup copies of files within atomic operations.
 *
 * Input:
 *  -a fully qualified file name to be copied from (from)
 *  -a fully qualified file name to be copied to (to)
 *
 * Return:
 *  -1 if there were no errors
 *  -0 otherwise
 */ 
int my_cp (char *from, char *to) {

  trace (NORM, default_trace, "my_cp from: %s  to: %s\n", from, to);
  return fastcopy (from, to, 0);
}

/* Restore all the files in (af->file_list) to their original state.
 *
 * restore_files () renames the files in (af->file_list) from *.bak
 * to * as part of an atomic file operation.  Files are backed up 
 * by renaming them to *.BAK_SFX and can be restored by renaming
 * without the BAK_SFX suffix.
 *
 * Input:
 *  -struct to allow messages to be passed to a UII user (uii)
 *  -struct which defines the list of files to be restored/renamed
 *   to their original names (af)
 *
 * Return:
 *  -1 if the operation was a success
 *  -0 otherwise
 */
int restore_files (uii_connection_t *uii, atom_finfo_t *af) {
  char tname[BUFSIZE+1];
  char *p, *q;

  if ((p = af->file_list) == NULL)
    return 0;

  for (q = strchr (p, '!'); q != NULL; p = q + 1, q = strchr (p, '!')) {
    *q = '\0';
    
    /* restore the DB cache file if one existed at the start of the operation */
    if (*p++ == 'y') {
      sprintf (tname, "%s.%s", p, BAK_SFX);
      if (!my_rename (tname, p, uii))
	return 0;
    }
    
    *q = '!';
  }

  return 1;
}

/* Remove/cleanup the backup files in (af->file_list).
 *
 * Function is expected to be called after a unsuccessful
 * atomic operation and it is desired to clean up the
 * uneeded files.
 *
 * File names in (af->tmp_list) are not fully qualified.
 * The (af->tmp_dir) is prepended to the names to form
 * a fully qualified file name.
 * 
 * Input:
 *  -list of temp files to be removed (af->tmp_list)
 *
 * Return:
 *  -1 if the entire list of files were removed
 *  -0 otherwise
 */
int rm_tmpfiles (atom_finfo_t *af) {
  char tname[BUFSIZE+1];
  char *p, *q;

  if ((p = af->tmp_list) == NULL)
    return 1;

  for (q = strchr (p, '!'); q != NULL; p = q + 1, q = strchr (p, '!')) {
    *q = '\0';
    
    /* remove the *.db from the tmp directory */
    sprintf (tname, "%s/%s", af->tmp_dir, p);
    remove (tname);

    *q = '!';
  }

  return 1;
}


/* See if (fname) exists in  directory (dir).
 *
 * Input:
 *  -a fully qualified directory path (dir)
 *  -a file name to look for in the directory (fname)
 *
 * Return:
 *  -1 if the file (fname) exists in (dir)
 *  -0 if (fname) does not exist in (dir)
 *  --1 if directory (dir) could not be opened for reading
 */
int file_exists (char *dir, char *fname) {
  int ret_code = 0;
  struct dirent *dp;
  DIR *dirp;

  /* sanity check */
  if (dir == NULL) {
    trace (ERROR, default_trace, 
	   "file_exists (): NULL directory string\n");
    return -1;
  }

  /* loop through the directory entries and see if we can find (fname) */
  if ((dirp = opendir (dir)) != NULL) {
    while ((dp = readdir (dirp)) != NULL)
      if (!strcmp (dp->d_name, fname)) {
	ret_code = 1;
	break;
      }
  }
  /* something went wrong, we couldn't open the directory for reading */
  else {
    trace (ERROR, default_trace, 
	   "file_exists (): Couldn't open dir (%s):(%s)\n", 
	   dir, strerror (errno));
    return -1;
  }

  closedir (dirp);

  return ret_code;
}

/* Remove/cleanup the backup files in (af->file_list).
 *
 * Function is expected to be called after a successful
 * atomic operation in which the back up files are no
 * longer needed.
 *
 * File names are assumed to be fully qualified.
 * 
 * The (af->file_list) string is in the 
 * following format, eg, "y/.../rgnet.db!n/.../RGNET.CURRENTSERIAL!\0"
 * The first character indicates if there was a backup file made.  If no
 * file existed originally then no backup file was made.  The '!' acts
 * as a delimiter between file names and the part in between is the fully
 * qualified file in which a backup was made.
 *
 * Input:
 *  -list of files for which backups were made (af->file_list)
 *
 * Return:
 *  -void
 */
void rm_backups (atom_finfo_t *af) {
  char tname[BUFSIZE], *p, *q;

  if ((p = af->file_list) == NULL)
    return;

  /* remove the backup files from our DB cache area */
  for (q = strchr (p, '!'); q != NULL; p = q + 1, q = strchr (p, '!')) {
    *q = '\0';

    /* remove the DB backup file if one existed at the start of the operation */
    if (*p++ == 'y') {
      sprintf (tname, "%s.%s", p, BAK_SFX);
      remove (tname);
    }

    *q = '!';
  }
}

