/* ====================================================================
 * Copyright (c) 1995 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 5. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * IT'S CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */


/*
 * URL counting
 *
 * Track all incoming requests for each URL.  Maintains a database
 * containing each URL accessed, count of times accessed, and last
 * time counter has been reset.  If the URL is a directory, then
 * the file that it is redirected to is counted.
 *
 * The values for count of times accessed, and last time counter was
 * reset are made available to the to the document via variables called
 * URL_COUNT and URL_COUNT_RESET, respectively.
 * Another variable URL_COUNT_DB is set to the database file used.
 *
 * Config file directives:
 *
 *     CounterAutoAdd        On or Off  Automatically add missing URL's
 *     CounterFile           path to ascii or dbm file (relative to logs/),
 *                           there is no default
 *     CounterTimeFmt        Time format for URL_CONTER_RESET
 *
 * Dan Kogai (dankogai@dan.co.jp)
 * Originally by Brian Kolaci (bk@galaxy.net)
 * Special Thanks to Sander Stefann (stafann@ndederland.net)
 *
 * $Id: mod_cntr.c,v 1.1 1999/07/12 13:53:08 dankogai Exp $
 *
 */


#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"

#ifdef OS2
#include <systems.h>
#endif

#ifdef HAVE_GDBM
#include <gdbm.h>
#define DBM_FILE GDBM_FILE
/* #define dbm_open gdbm_open */
#define dbm_fetch gdbm_fetch
#define dbm_store gdbm_store
#define dbm_close gdbm_close
#define DBM_REPLACE GDBM_REPLACE
#else
#include <ndbm.h>
#define DBM_FILE DBM *
#endif

////#define DEBUG_CGI

module cntr_module;

char *cntr_uri_to_path(request_rec *r, const char *full_uri)
{
    uri_components parsed;
    ap_parse_uri_components(r->pool, full_uri, &parsed);
    return parsed.path;
}

/*
 *  Data structions
 */

#include <time.h>

typedef struct {
    unsigned long count;
    time_t date;
}      cntr_results;

typedef struct {
    int cntr_default;
    int cntr_auto_add;
    char *cntr_file;
    char *cntr_timefmt;
    char *cntr_facedir;
}      cntr_config_rec;


#define DEFAULT_FACE	"default"

#define	DEF_CNTR_AA	 1
#define	DEF_CNTR_FILE	 2
#define	DEF_CNTR_TIMEFMT 4
#define	DEF_CNTR_FACEDIR 8
#define	DEF_ALL          ~0

/*
 *  Set defaults
 */

#define DEFAULT_CNTR_FILE	""
#define DEFAULT_CNTR_TIMEFMT	"%A, %d-%b-%Y %H:%M:%S %Z"
#define DEFAULT_CNTR_FACEDIR    "/usr/local/apache/share/digits"

/*
 * Create config data structure
 */
void *create_cntr_config_rec(pool *p, char *d)
{
    /*
     * Set the defaults
     */
    cntr_config_rec *rec =
    (cntr_config_rec *) ap_pcalloc(p, sizeof(cntr_config_rec));
    rec->cntr_auto_add = 0;
    rec->cntr_file = DEFAULT_CNTR_FILE ? ap_pstrdup(p, DEFAULT_CNTR_FILE) : NULL;
    rec->cntr_timefmt = ap_pstrdup(p, DEFAULT_CNTR_TIMEFMT);
    rec->cntr_facedir = ap_pstrdup(p, DEFAULT_CNTR_FACEDIR);
    rec->cntr_default = DEF_ALL;
    return (rec);
}

void *merge_config_rec(pool *p, void *parent, void *sub)
{
    cntr_config_rec *par = (cntr_config_rec *) parent;
    cntr_config_rec *chld = (cntr_config_rec *) sub;
    cntr_config_rec *mrg = (cntr_config_rec *) ap_palloc(p, sizeof(*mrg));

    if (chld->cntr_default & DEF_CNTR_AA)
	mrg->cntr_auto_add = par->cntr_auto_add;
    else
	mrg->cntr_auto_add = chld->cntr_auto_add;

    if (chld->cntr_default & DEF_CNTR_FILE)
	mrg->cntr_file = par->cntr_file;
    else
	mrg->cntr_file = chld->cntr_file;

    if (chld->cntr_default & DEF_CNTR_TIMEFMT)
	mrg->cntr_timefmt = par->cntr_timefmt;
    else
	mrg->cntr_timefmt = chld->cntr_timefmt;

    if (chld->cntr_default & DEF_CNTR_FACEDIR)
	mrg->cntr_facedir = par->cntr_facedir;
    else
	mrg->cntr_facedir = chld->cntr_facedir;

    mrg->cntr_default = 0;
    return (mrg);
}

const char *set_cntr_autoadd(cmd_parms *cmd, void *ct, int arg)
{
    cntr_config_rec *conf = (cntr_config_rec *) ct;
    conf->cntr_auto_add = arg;
    conf->cntr_default &= ~DEF_CNTR_AA;
    return (NULL);
}

const char *set_cntr_file(cmd_parms *cmd, void *ct, char *arg)
{
    void *ret = NULL;
    cntr_config_rec *conf = (cntr_config_rec *) ct;

    if (strcmp(arg, "/dev/null"))
	conf->cntr_file = ap_server_root_relative(cmd->pool, arg);
    else
	conf->cntr_file = "";
    conf->cntr_default &= ~DEF_CNTR_FILE;

    return (ret);
}

const char *set_cntr_timefmt(cmd_parms *cmd, void *ct, char *arg)
{
    void *ret = NULL;
    cntr_config_rec *conf = (cntr_config_rec *) ct;

    conf->cntr_timefmt = arg;
    conf->cntr_default &= ~DEF_CNTR_TIMEFMT;

    return (ret);
}

const char *set_cntr_facedir(cmd_parms *cmd, void *ct, char *arg)
{
    void *ret = NULL;
    cntr_config_rec *conf = (cntr_config_rec *) ct;

    conf->cntr_facedir = arg;
    conf->cntr_default &= ~DEF_CNTR_FACEDIR;

    return (ret);
}

/* to snatch module config from mod_dir.c */

typedef struct dir_config_struct {
    array_header *index_names;
}                 dir_config_rec;

char *set_url_count_dindex(request_rec *r)
{
    char buf[MAXPATHLEN] = "";
    dir_config_rec *dir =
    (dir_config_rec *) ap_get_module_config(r->per_dir_config,
					 ap_find_linked_module("mod_dir.c"));
    if (dir && dir->index_names) {
	char **names_ptr = (char **) dir->index_names->elts;
	int num_names = dir->index_names->nelts;
	for (; num_names; ++names_ptr, --num_names) {
	    if (!*buf) {
		strcpy(buf, *names_ptr);
	    }
	    else {
		strcat(buf, " ");
		strcat(buf, *names_ptr);
	    }
	}
    }
    return ap_pstrdup(r->pool, buf);
}

command_rec cntr_cmds[] = {
    {"CounterAutoAdd", set_cntr_autoadd, NULL, OR_ALL, FLAG,
    "When set, automatically add new URL to counter file"},
    {"CounterFile", set_cntr_file, NULL, OR_ALL, TAKE1,
    "Name of counter file or database"},
    {"CounterTimeFmt", set_cntr_timefmt, NULL, OR_ALL, TAKE1,
    "Time Format for URL_COUNT_RESET"},
    {"CounterFaceDir", set_cntr_facedir, NULL, OR_ALL, TAKE1,
    "Face Directory"},
    {NULL}
};

/*
 * Handle special cases of counter-filename.
 * Case 1: %v in the filename will be expanded to the server-name.
 * by Sander Steffann
 */

char *cntr_construct_filename(request_rec *r, char *p, char *t, int l)
{
    int i = 0;
    int j = 0;
    int k;

    if (p == NULL)
	return NULL;

    bzero(t, l);
    while ((p[i] != 0) && (j < l)) {
	if ((p[i] == '%') && (p[i + 1] == 'v')) {
	    i += 2;		/* Skip %v */

	    k = 0;
	    while ((r->server->server_hostname[k] != 0) && (j < l)) {
		t[j++] = r->server->server_hostname[k++];
	    }
	}
	else {
	    /* Copy character and continue with next */
	    t[j++] = p[i++];
	}
    }

    if (j < l) {
	t[j] = 0;
	return t;
    }
    else {
        //originally t[0] == 0;
        t[0] = 0;
	return NULL;
    }
}

/*
 * open dbm with file lock.
 */
DBM_FILE do_dbm_open(request_rec *r, char *cntr_file, char *err)
{
    DBM_FILE dbm;
    char pcntr_file[MAXPATHLEN];
    cntr_construct_filename(r, cntr_file, pcntr_file, sizeof(pcntr_file));
#ifdef HAVE_DB
    if ((dbm = dbm_open(pcntr_file, O_RDWR | O_CREAT | O_EXLOCK, 0666))
	== NULL) {
	sprintf(err, "Failed to open counter dbmfile: %s", pcntr_file);
    }
#elif HAVE_GDBM
    if ((dbm = gdbm_open(pcntr_file, 512, GDBM_WRCREAT, 0666, 0)) == NULL) {
	sprintf(err, "Failed to open counter dbmfile: %s",
		gdbm_strerror(gdbm_errno));
    }
#else
    if ((dbm = dbm_open(pcntr_file, O_RDWR | O_CREAT, 0666)) != NULL) {
	sprintf(err, "Failed to open counter dbmfile: %s", pcntr_file);
    }
    else{
       int lockerr;
       struct flock lock = {F_WRLCK,0,0,0};
    while ((lockerr = fcntl(dbm_dirfno(dbm), F_SETLKW, &lock)) < 0
	   && errno == EINTR) {
	continue;
    }
    if (lockerr) {
	dbm_close(dbm);
	sprintf(err, "Failed to lock DBM counter file: %s ", pcntr_file);
    }
    }
#endif
    return dbm;
}

void do_dbm_close(DBM_FILE dbm)
{
#if defined (HAVE_DB) || defined (HAVE_GDBM)
    /* unlocking is automatically taken care of */
    dbm_close(dbm);
#else
    struct flock unlock = {F_UNLCK, 0, 0, 0};
    fcntl(dbm_dirfno(dbm), F_SETLKW, &unlock);
    dbm_close(dbm);
#endif				/* HAVE_DB || HAVE_GDBM */
}


/*
 * Bare-text counter is no longer suppported.
 * -- dankogai
 *
 char*	cntr_incfile( pool* p, cntr_results* results,
 cntr_config_rec* r, char* uri )
*/

char *cntr_incdbm(request_rec *r, cntr_results * results,
		       cntr_config_rec * c, char *uri)
{
    DBM_FILE dbm;
    char err[256];
    datum d, q;

    results->count = 0;
    results->date = 0;

    q.dptr = uri;
    q.dsize = strlen(q.dptr);

    if ((dbm = do_dbm_open(r, c->cntr_file, err)) == NULL) {
	return ap_pstrcat(r->pool, err, NULL);
    }

    d = dbm_fetch(dbm, q);

    /*
     * If found, inclement the counter
     */
    if (d.dptr) {
	memcpy(results, d.dptr, sizeof(cntr_results));
	results->count++;
    }
    /*
     * Else create a new one;
     */
    else {
	results->count = 1;
	results->date = time(0L);
    }
    /*
     * Add or update the record
     */
    if (d.dptr || c->cntr_auto_add) {
	d.dptr = (void *) results;
	d.dsize = sizeof(cntr_results);
	dbm_store(dbm, q, d, DBM_REPLACE);
    }

    do_dbm_close(dbm);

    return (OK);
}

char *cntr_inc(request_rec *r, cntr_results * results,
	            cntr_config_rec * c, char *uri)
{
    /* Normalize the URI stripping out double "//" */
    char *puri = ap_pstrdup(r->pool, uri);
    char *ptr = puri;
    while (ptr && *ptr) {
	if (*ptr == '/' && *(ptr + 1) == '/') {
	    char *q = ptr + 1;
	    while (*q = *(q + 1))
		q++;
	}
	else {
	    ptr++;
	}
    }

    return cntr_incdbm(r, results, c, puri);
}

/*******************************************************************\
 *                                                                 *
 * Conceived on VII June MCMXCIX ( or should that be MIM ? )       *
 *                                                                 *
 *******************************************************************/
const char *roman( unsigned n, pool *p )
{
   static char* rom[3][10] = {
      {"","C","CC","CCC","CD","D","DC","DCC","DCCC","CM"},
      {"","X","XX","XXX","XL","L","LX","LXX","LXXX","XC"},
      {"","I","II","III","IV","V","VI","VII","VIII","IX"}
   };
      
   int mille = n / 1000;
   int rest  = n % 1000;

   char *q, *ret = ap_pcalloc( p, mille + 16 );
   for( q=ret; mille--; )
   {
      *q++ = 'M';
   }
   mille = rest / 100;
   rest %= 100;
   strcat( ret, rom[0][mille] );

   mille = rest / 10;
   rest %= 10;
   strcat( ret, rom[1][mille] );
   strcat( ret, rom[2][rest] );

   return ret;
}

int cntr_update(request_rec *r)
{
    char *err;
    char buf[MAXPATHLEN];
    ////char *timefmt;
    ////char *facedir;

    int ret = OK;
    cntr_config_rec *conf;
    cntr_results *res;

    /*
     * Get actual file, if locally redirected
     */
    while (r->next)
	r = r->next;
    /*
     * Skip if missing URI or this is an included request
     */
    if (!r->uri || !strcmp(r->protocol, "INCLUDED"))
	return (DECLINED);
    if (!S_ISREG(r->finfo.st_mode))
	return (DECLINED);

    /*
     * get configuration
     */
    conf = ap_get_module_config(r->per_dir_config, &cntr_module);
    res = (cntr_results *) ap_pcalloc(r->pool, sizeof(cntr_results));
    /*
     * skip if there is no counter file;
     */
    if (!*conf->cntr_file) {
	return (DECLINED);
    }
    else {
	if (err = cntr_inc(r, res, conf, r->uri)) {
	    ap_log_error_old(err, r->server);
	}
    }

    /*
     * set environment variables
     */

    sprintf(buf, "%lu", res->count);
    ap_table_set(r->subprocess_env, "URL_COUNT", buf);
    /*
     * Now for the romans...
     */
    if( res->count < 50000 )
       ap_table_set(r->subprocess_env, "VRL_COVNT", roman(res->count, r->pool));
    else
       ap_table_set(r->subprocess_env, "VRL_COVNT", "Ouch ! Romans can't count <B>THAT</B> high");
    
    ap_table_set(r->subprocess_env, "URL_COUNT_RESET",
	      ap_ht_time(r->pool, res->date, conf->cntr_timefmt, 0));
    ap_table_set(r->subprocess_env, "URL_COUNT_DB",
	      cntr_construct_filename(r, conf->cntr_file, buf, sizeof(buf)));
    ap_table_set(r->subprocess_env, "URL_COUNT_FACEDIR", conf->cntr_facedir);
    ap_table_set(r->subprocess_env, "URL_COUNT_TIMEFMT", conf->cntr_timefmt);
    ap_table_set(r->subprocess_env, "URL_COUNT_DINDEX", set_url_count_dindex(r));
    return ret;
}

/*
 *---- handler part---
 */

int cntr_parse_query(request_rec *r,
	     char *query, char **face, int *ndigit, int *trans, int *fcount)
{
    char *qbuf, *q;

    if (!query || !*query) {
	return OK;
    }

    qbuf = ap_pstrdup(r->pool, query);
    q = strtok(qbuf, "&");

    do {
	if (strncasecmp(q, "face=", 5) == 0) {
	    *face = ap_pstrdup(r->pool, q + 5);

         /******************************************
          *                                        *
          * 'Ere we go !!!!!! (handle "random")    *
          *                                        *
          ******************************************/
         if( !strcasecmp(*face, "random") ){
            cntr_config_rec *c =
               (cntr_config_rec *) ap_get_module_config(r->per_dir_config, &cntr_module);
            DIR *facedir = opendir( c->cntr_facedir );
            if( facedir ){
               struct dirent *direntry;
               int i;
               
               while( direntry = readdir( facedir ) ){
                  if( *(direntry->d_name) == '.' )
                     continue;
                  ap_table_add( r->notes, direntry->d_name, "d" );
               } /* wend */

               if( (i=((array_header*)r->notes)->nelts) ){
                  table_entry *t = (table_entry*)((array_header*)r->notes)->elts;
                  *face = ap_pstrdup( r->pool, t[rand() % i].key );
               }
            }
            /* else (!facedir) nothing */
         }
	}
	else if (strncasecmp(q, "ndigit=", 7) == 0) {
	    *ndigit = atoi(q + 7);
	}
	else if (strncasecmp(q, "trans", 5) == 0) {
	    *trans = 1;
	}
	else if (strncasecmp(q, "fcount=", 7) == 0) {
	    *fcount = atoi(q + 7);
	}
    } while (q = strtok(NULL, "&"));
    return OK;
}

/*
 * gdmini.h needed
 */

#include "gdmini.h"

gdImagePtr cntr_read_digit(request_rec *r, int digit)
{
    char file[256];
    gdImagePtr im = NULL;
    FILE *fp;

    sprintf(file, "%d.gif", digit);
    if (fp = ap_pfopen(r->pool, file, "r")) {
	im = gdImageCreateFromGif(fp);
	ap_pfclose(r->pool, fp);
    }
    else {
	ap_log_error_old(ap_psprintf(r->pool, "%d.gif:%s\n", digit, strerror(errno)),
		  r->server);
    }
    return im;
}

#define MAXNDIGIT 12

int cntr_draw_digit(request_rec *r, cntr_config_rec * c, int count)
{
    int i;
    ////int resize = 0;
    int width = 0;
    int height = 0;
    char digitbuf[256], digitfmt[16], *dp;
    gdImagePtr imdigit[10] = {NULL};
    gdImagePtr imgd;
    dynamicPtr *imdyna;
    char *face = NULL;
    int ndigit = 0;
    int trans = 0;
    int fcount = 0;

    if (r->args) {
	cntr_parse_query(r, r->args, &face, &ndigit, &trans, &fcount);
    }

    if (ndigit > 0) {
	sprintf(digitfmt, "%s%dd", "%0",
		(ndigit <= MAXNDIGIT ? ndigit : MAXNDIGIT));
    }
    else {
	strcpy(digitfmt, "%d");
    }
    sprintf(digitbuf, digitfmt, (fcount ? fcount : count));

    /*
     * Load required digits Calculate size of output imgd too
     */

    /* Go to digits directory */
    if (chdir(c->cntr_facedir) != 0) {
	ap_log_error_old(ap_psprintf(r->pool, "%s:%s\n", c->cntr_facedir,
			   strerror(errno)), r->server);
	goto cleanup;
    }
    if (face) {
	if (chdir(face) != 0) {
	    ap_log_error_old(ap_psprintf(r->pool, "%s:%s. Trying %s\n",
			    c->cntr_facedir, strerror(errno), DEFAULT_FACE),
		      r->server);
	    if (chdir(DEFAULT_FACE) != 0) {
		ap_log_error_old(ap_psprintf(r->pool, "%s:%s\n", strerror(errno), DEFAULT_FACE),
			  r->server);
		goto cleanup;
	    }
	}
    }
    else {
	if (chdir(DEFAULT_FACE) != 0) {
	    ap_log_error_old(ap_psprintf(r->pool, "%s:%s\n", strerror(errno), DEFAULT_FACE),
		      r->server);
	    goto cleanup;
	}
    }

    for (dp = digitbuf; dp && *dp; dp++) {
	if (isdigit(*dp)) {
	    i = *dp - '0';
	    if (imdigit[i] == NULL)
		if ((imdigit[i] = cntr_read_digit(r, i)) == NULL) {
		    goto cleanup;
		}
	    if (gdImageSY(imdigit[i]) > height)
		height = gdImageSY(imdigit[i]);
	    width += gdImageSX(imdigit[i]);
	}
    }

    /* Create output image */
    if ((imgd = gdImageCreate(width, height)) == NULL) {
	goto cleanup;
    }

    /* Draw rest of digits */
    width = 0;
    for (dp = digitbuf; dp && *dp; dp++) {
	if (isdigit(*dp)) {
	    i = *dp - '0';
	    gdImageCopyResized(imgd, imdigit[i], width, 0, 0, 0,
			       imdigit[i]->sx, height,
			       imdigit[i]->sx, imdigit[i]->sy);
	    width += gdImageSX(imdigit[i]);
	}
    }

    /* Make output interlaced */
    gdImageInterlace(imgd, 1);
    if (trans) {
	gdImageColorTransparent(imgd, gdImageGetPixel(imgd, 0, 0));
    }
    /* Spit out image */

    imdyna = gdImageGifData(imgd);

    r->content_type = "image/gif";
    ap_table_set(r->headers_out, "Pragma", "no-cache");
    ap_table_set(r->headers_out, "Expires", "Thursday, 01-Jan-1970 00:00:00");
    ap_send_http_header(r);
    ap_rwrite(imdyna->data, imdyna->logicalSize, r);

    freeDynamic(imdyna);
    gdImageDestroy(imgd);
    goto cleanup;

cleanup:
    for (i = 0; i < 10; i++) {
	if (imdigit[i] != NULL) {
	    gdImageDestroy(imdigit[i]);
	}
    }
    return OK;
}

void dump_table(request_rec *r, table *t)
{
    array_header *ah = (array_header *) t;
    table_entry *te = (table_entry *) ah->elts;
    int i;
    ap_rprintf(r, "<pre>\n");
    for (i = 0; i < ah->nelts; i++) {
	ap_rprintf(r, "%s = %s\n", te[i].key, te[i].val);
    }
    ap_rprintf(r, "</pre>\n");
}

int cntr_lookup(request_rec *r, cntr_config_rec * c,
		    const char *uri, cntr_results * counter)
{
    int result = 0;
    DBM_FILE dbm;
    datum d, q;

    char pcntr_file[MAXPATHLEN];

#ifdef DEBUG_CGI
#ifdef OS2
    /* Under OS/2 need to use device con. */
    FILE *dbg = fopen("con", "wt");
#else
    FILE *dbg = fopen("/dev/tty", "w");
#endif
#endif

#ifdef DEBUG_CGI
    fprintf( dbg, "cntr_lookup - URI: %s\n", uri );
#endif

    cntr_construct_filename(r, c->cntr_file, pcntr_file, sizeof(pcntr_file));

    q.dptr = (char *) uri;
    q.dsize = strlen(q.dptr);

    /*
     * No locking is necessary for read_only. so no do_dbm_open is done.
     */
#ifdef HAVE_GDBM
    if ((dbm = gdbm_open(pcntr_file, 512, GDBM_READER, 0444, 0)) == NULL) {
	ap_log_error_old(ap_psprintf(r->pool, "Failed to open %s", pcntr_file), r->server);
    #ifdef DEBUG_CGI
        fclose( dbg );
    #endif
	return 0;
    }
#else
    if ((dbm = dbm_open(pcntr_file, O_RDONLY, 0444)) == NULL) {
	ap_log_error_old(ap_psprintf(r->pool, "Failed to open %s", pcntr_file), r->server);
    #ifdef DEBUG_CGI
        fclose( dbg );
    #endif
	return 0;
    }
#endif
    d = dbm_fetch(dbm, q);

    if (d.dptr) {		/* found */
	memcpy(counter, d.dptr, sizeof(cntr_results));
	result = counter->count;
    }
    else {
	if (uri[strlen(uri) - 1] == '/') {	/* if directory */
	    char new_uri[MAXPATHLEN];
	    request_rec *rr = ap_sub_req_lookup_uri(uri, r);

	    dir_config_rec *dir =
	    (dir_config_rec *) ap_get_module_config(rr->per_dir_config,
					   ap_find_linked_module("mod_dir.c"));
	    ap_destroy_sub_req(rr);

	    if (dir && dir->index_names) {
		char **names_ptr = (char **) dir->index_names->elts;
		int num_names = dir->index_names->nelts;
		for (; num_names; ++names_ptr, --num_names) {
		    sprintf(new_uri, "%s%s", uri, *names_ptr);
		    q.dptr = new_uri;
		    q.dsize = strlen(q.dptr);
		    d = dbm_fetch(dbm, q);
		    if (d.dptr) {
			memcpy(counter, d.dptr, sizeof(cntr_results));
			result = counter->count;
		    }
		}
	    }
	}
	else {
	    ap_log_error_old("No uri found\n", r->server);
	}
    }

#ifdef DEBUG_CGI
    fclose( dbg );
#endif
    dbm_close(dbm);

    return result;
}

int cntr_handler(request_rec *r)
{
    cntr_config_rec *c =
    (cntr_config_rec *) ap_get_module_config(r->per_dir_config, &cntr_module);

    ////char *uri = r->uri;
    const char *referer = ap_table_get(r->headers_in, "Referer");
    char *path_info = r->path_info;
    ////char *args = r->args;
    ////char *face = NULL;
    ////int ndigit = 0;
    ////int trans = 0;

    cntr_results counter;
    counter.count = 0;
    counter.date = 0;

    if (!c->cntr_file) {
	return DECLINED;
    }

    if (!r->header_only) {
	if (strlen(path_info)) {
	    cntr_lookup(r, c, path_info, &counter);
	}
	else if (referer) {
	    cntr_lookup(r, c, cntr_uri_to_path(r, referer), &counter);
	}

        /***************************************
         *   For the benefit of face=random    *
         ***************************************/
        srand( r->request_time );
        
	return cntr_draw_digit(r, c, counter.count);
    }
    else {
	return DECLINED;
    }
}

int cntr_debug_handler(request_rec *r)
{
    cntr_config_rec *c =
    (cntr_config_rec *) ap_get_module_config(r->per_dir_config, &cntr_module);

    char *uri = r->uri;
    const char *referer = ap_table_get(r->headers_in, "Referer");
    char *path_info = r->path_info;
    char *args = r->args;
    char *face = NULL;
    int ndigit = 0;
    int trans = 0;
    int fcount = 0;

    cntr_results counter;
    counter.count = 0;
    counter.date = 0;

    r->content_type = "text/html";
    ap_send_http_header(r);

    if (!r->header_only) {
	ap_rprintf(r, "<h2>Configuration:</h2>\n");
	ap_rprintf(r, "<pre>\n");
	ap_rprintf(r, "cntr_auto_add = %d\n", c->cntr_auto_add);
	ap_rprintf(r, "cntr_file = %s\n", c->cntr_file);
	ap_rprintf(r, "cntr_timefmt = %s\n", c->cntr_timefmt);
	ap_rprintf(r, "cntr_facedir = %s\n", c->cntr_facedir);
	ap_rprintf(r, "</pre>\n");

	ap_rprintf(r, "<h2>Request:</h2>\n");
	ap_rprintf(r, "<pre>\n");
	ap_rprintf(r, "uri = %s\n", uri);
	ap_rprintf(r, "referer = %s\n", referer);
	ap_rprintf(r, "path_info = %s\n", path_info);
	ap_rprintf(r, "args = %s\n", r->args);
	ap_rprintf(r, "</pre>\n");

	ap_rprintf(r, "<h2>Counter:</h2>\n");
	ap_rprintf(r, "<pre>\n");

	if (strlen(path_info)) {
	    ap_rprintf(r, "<b>count(path_info)</b>\n");
	    cntr_lookup(r, c, path_info, &counter);
	    ap_rprintf(r, " count = %ld\n date = %ld\n",
		    counter.count, counter.date);
	}
	if (referer) {
	    ap_rprintf(r, "<b>count(referer)</b>\n");
	    cntr_lookup(r, c, cntr_uri_to_path(r, referer), &counter);
	    ap_rprintf(r, " count = %ld\n date = %ld\n",
		    counter.count, counter.date);
	}
	ap_rprintf(r, "</pre>\n");

	ap_rprintf(r, "<h2>Queries:</h2>\n");
	ap_rprintf(r, "<pre>\n");
	if (args) {
	    cntr_parse_query(r, args, &face, &ndigit, &trans, &fcount);
	    ap_rprintf(r, "face   = %s\n", face);
	    ap_rprintf(r, "ndigit = %d\n", ndigit);
	    ap_rprintf(r, "trans  = %d\n", trans);
	    ap_rprintf(r, "trans  = %d\n", fcount);
	}
	ap_rprintf(r, "</pre>\n");

	ap_rprintf(r, "<h2>headers_in</h2>\n");
	dump_table(r, r->headers_in);
    }
    return OK;
}

handler_rec cntr_handlers[] =
{
    {"server-cntr", cntr_handler},
    {"server-cntr-debug", cntr_debug_handler},
    {NULL}
};


module cntr_module = {
    STANDARD_MODULE_STUFF,
    NULL,			/* initializer */
    create_cntr_config_rec,	/* dir config creater */
    merge_config_rec,		/* dir merge --- default is to override */
    NULL,			/* server config */
    NULL,			/* merge server config */
    cntr_cmds,			/* command table */
    cntr_handlers,		/* handlers */
    NULL,			/* filename translation */
    NULL,			/* ap_check_user_id */
    NULL,			/* check auth */
    NULL,			/* check access */
    NULL,			/* type_checker */
    cntr_update,		/* fixups */
    NULL			/* logger */
};

