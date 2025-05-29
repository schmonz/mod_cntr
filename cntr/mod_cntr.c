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
 * time counter has been reset.
 *
 * Dan Kogai (dankogai@dan.co.jp)
 * Originally by Brian Kolaci (bk@galaxy.net)
 * Special Thanks to Sander Stefann (stafann@ndederland.net)
 *
 * $Id: mod_cntr.c,v 1.1 1999/07/12 13:53:08 dankogai Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>

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

/*
 *  Data structions
 */

#include <time.h>

typedef struct {
    unsigned long count;
    time_t date;
}      cntr_results;

typedef struct {
    int cntr_auto_add;
    char *cntr_file;
    char *cntr_timefmt;
    char *cntr_facedir;
}      cntr_config_rec;

#define DEFAULT_FACE	"default"

/*
 *  Set defaults
 */
#define DEFAULT_CNTR_FILE	""
#define DEFAULT_CNTR_TIMEFMT	"%A, %d-%b-%Y %H:%M:%S %Z"
#define DEFAULT_CNTR_FACEDIR    "/usr/local/apache/share/digits"

/*
 * Create config data structure
 */
cntr_config_rec *global_config = NULL;

char *ap_pstrdup(const char *s)
{
    if (!s) return NULL;
    return strdup(s);
}

/*
 * Initialize configuration from environment variables
 */
cntr_config_rec *init_config()
{
    cntr_config_rec *conf = calloc(1, sizeof(cntr_config_rec));
    if (!conf) return NULL;

    char *env_val;

    env_val = getenv("CNTR_AUTO_ADD");
    conf->cntr_auto_add = (env_val && !strcasecmp(env_val, "on")) ? 1 : 0;

    env_val = getenv("CNTR_FILE");
    conf->cntr_file = env_val ? ap_pstrdup(env_val) : ap_pstrdup(DEFAULT_CNTR_FILE);

    env_val = getenv("CNTR_TIMEFMT");
    conf->cntr_timefmt = env_val ? ap_pstrdup(env_val) : ap_pstrdup(DEFAULT_CNTR_TIMEFMT);

    env_val = getenv("CNTR_FACEDIR");
    conf->cntr_facedir = env_val ? ap_pstrdup(env_val) : ap_pstrdup(DEFAULT_CNTR_FACEDIR);

    return conf;
}

void cleanup_config(cntr_config_rec *conf)
{
    if (conf) {
        free(conf->cntr_file);
        free(conf->cntr_timefmt);
        free(conf->cntr_facedir);
        free(conf);
    }
}

/*
 * open dbm with file lock.
 */
DBM_FILE do_dbm_open(char *pcntr_file, char *err)
{
    DBM_FILE dbm;
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

char *cntr_incdbm(cntr_results * results,
		       cntr_config_rec * c, char *uri)
{
    DBM_FILE dbm;
    char err[256];
    datum d, q;

    results->count = 0;
    results->date = 0;

    q.dptr = uri;
    q.dsize = strlen(q.dptr);

    if ((dbm = do_dbm_open(c->cntr_file, err)) == NULL) {
	return ap_pstrdup(err);
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
    return NULL;
}

char *cntr_inc(cntr_results * results,
	            cntr_config_rec * c, char *uri)
{
    /* Normalize the URI stripping out double "//" */
    char *puri = ap_pstrdup(uri);
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

    char *result = cntr_incdbm(results, c, puri);
    free(puri);
    return result;
}

/*******************************************************************\
 *                                                                 *
 * Conceived on VII June MCMXCIX ( or should that be MIM ? )       *
 *                                                                 *
 *******************************************************************/
const char *roman( unsigned n )
{
   static char* rom[3][10] = {
      {"","C","CC","CCC","CD","D","DC","DCC","DCCC","CM"},
      {"","X","XX","XXX","XL","L","LX","LXX","LXXX","XC"},
      {"","I","II","III","IV","V","VI","VII","VIII","IX"}
   };
      
   int mille = n / 1000;
   int rest  = n % 1000;

   static char ret[1024];
   char *q;

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

/*
 *---- handler part---
 */

int cntr_parse_query(
	     char *query, char **face, int *ndigit, int *trans, int *fcount)
{
    char *qbuf, *q;

    if (!query || !*query) {
	return 0;
    }

    qbuf = ap_pstrdup(query);
    q = strtok(qbuf, "&");

    do {
	if (strncasecmp(q, "face=", 5) == 0) {
	    *face = ap_pstrdup(q + 5);

         /******************************************
          *                                        *
          * 'Ere we go !!!!!! (handle "random")    *
          *                                        *
          ******************************************/
         if( !strcasecmp(*face, "random") ){
            DIR *facedir = opendir( global_config->cntr_facedir );
            if( facedir ){
               struct dirent *direntry;
               char *faces[256];
               int face_count = 0;
               
               while( direntry = readdir( facedir ) ){
                  if( *(direntry->d_name) == '.' )
                     continue;
                  if(face_count < 256) {
                      faces[face_count++] = ap_pstrdup(direntry->d_name);
                  }
               } /* wend */
               closedir(facedir);

               if(face_count > 0) {
                  free(*face);
                  *face = ap_pstrdup(faces[rand() % face_count]);

                  for(int i = 0; i < face_count; i++) {
                      free(faces[i]);
                  }
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
    free(qbuf);
    return 0;
}

/*
 * gdmini.h needed
 */

#include "gdmini.h"

gdImagePtr cntr_read_digit(int digit)
{
    char file[256];
    gdImagePtr im = NULL;
    FILE *fp;

    sprintf(file, "%d.gif", digit);
    if (fp = fopen(file, "r")) {
	im = gdImageCreateFromGif(fp);
	fclose(fp);
    }
    else {
	fprintf(stderr, "%d.gif: %s\n", digit, strerror(errno));
    }
    return im;
}

#define MAXNDIGIT 12

int cntr_draw_digit(cntr_config_rec * c, int count)
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

    char *query_string = getenv("QUERY_STRING");

    if (query_string) {
	cntr_parse_query(query_string, &face, &ndigit, &trans, &fcount);
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
	fprintf(stderr, "%s:%s\n", c->cntr_facedir,
			   strerror(errno));
	goto cleanup;
    }
    if (face) {
	if (chdir(face) != 0) {
	    fprintf(stderr, "%s:%s. Trying %s\n",
			    c->cntr_facedir, strerror(errno), DEFAULT_FACE);
	    if (chdir(DEFAULT_FACE) != 0) {
		fprintf(stderr, "%s:%s\n", strerror(errno), DEFAULT_FACE);
		goto cleanup;
	    }
	}
    }
    else {
	if (chdir(DEFAULT_FACE) != 0) {
	    fprintf(stderr, "%s:%s\n", strerror(errno), DEFAULT_FACE);
	    goto cleanup;
	}
    }

    for (dp = digitbuf; dp && *dp; dp++) {
	if (isdigit(*dp)) {
	    i = *dp - '0';
	    if (imdigit[i] == NULL)
		if ((imdigit[i] = cntr_read_digit(i)) == NULL) {
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

    printf("Content-Type: image/gif\r\n");
    printf("Pragma: no-cache\r\n");
    printf("Expires: Thursday, 01-Jan-1970 00:00:00\r\n");
    printf("\r\n");
    fwrite(imdyna->data, imdyna->logicalSize, 1, stdout);

    freeDynamic(imdyna);
    gdImageDestroy(imgd);
    goto cleanup;

cleanup:
    for (i = 0; i < 10; i++) {
	if (imdigit[i] != NULL) {
	    gdImageDestroy(imdigit[i]);
	}
    }

    if (face) free(face);
    return 0;
}

int cntr_lookup(cntr_config_rec * c,
		    const char *uri, cntr_results * counter)
{
    int result = 0;
    DBM_FILE dbm;
    datum d, q;

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

    q.dptr = (char *) uri;
    q.dsize = strlen(q.dptr);

    /*
     * No locking is necessary for read_only. so no do_dbm_open is done.
     */
#ifdef HAVE_GDBM
    if ((dbm = gdbm_open(c->cntr_file, 512, GDBM_READER, 0444, 0)) == NULL) {
	fprintf(stderr, "Failed to open %s\n", c->cntr_file);
    #ifdef DEBUG_CGI
        fclose( dbg );
    #endif
	return 0;
    }
#else
    if ((dbm = dbm_open(c->cntr_file, O_RDONLY, 0444)) == NULL) {
	fprintf(stderr, "Failed to open %s\n", c->cntr_file);
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

#ifdef DEBUG_CGI
    fclose( dbg );
#endif
    dbm_close(dbm);
    return result;
}

/*
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

        /////////////////////////////////////////
         *   For the benefit of face=random    *
         ////////////////////////////////////////
        srand( r->request_time );
        
	return cntr_draw_digit(r, c, counter.count);
    }
    else {
	return DECLINED;
    }
}
*/

int cntr_debug_handler(cntr_config_rec *c)
{
    char *path_info = getenv("PATH_INFO");
    char *referer = getenv("HTTP_REFERER");
    char *query_string = getenv("QUERY_STRING");
    char *request_uri = getenv("REQUEST_URI");
    char *face = NULL;
    int ndigit = 0;
    int trans = 0;
    int fcount = 0;

    cntr_results counter;
    counter.count = 0;
    counter.date = 0;

    printf("Content-Type: text/html\r\n\r\n");

    printf("<h2>Configuration:</h2>\n");
    printf("<pre>\n");
    printf("cntr_auto_add = %d\n", c->cntr_auto_add);
    printf("cntr_file = %s\n", c->cntr_file);
    printf("cntr_timefmt = %s\n", c->cntr_timefmt);
    printf("cntr_facedir = %s\n", c->cntr_facedir);
    printf("</pre>\n");

    printf("<h2>Request:</h2>\n");
    printf("<pre>\n");
    printf("request_uri = %s\n", request_uri ? request_uri : "(null)");
    printf("referer = %s\n", referer ? referer : "(null)");
    printf("path_info = %s\n", path_info ? path_info : "(null)");
    printf("query_string = %s\n", query_string ? query_string : "(null)");
    printf("</pre>\n");

    printf("<h2>Counter:</h2>\n");
    printf("<pre>\n");

    if (path_info && strlen(path_info)) {
	printf("<b>count(path_info)</b>\n");
	cntr_lookup(c, path_info, &counter);
	printf(" count = %ld\n date = %ld\n", counter.count, counter.date);
    }

    printf("</pre>\n");

    printf("<h2>Queries:</h2>\n");
    printf("<pre>\n");
    if (query_string) {
	cntr_parse_query(query_string, &face, &ndigit, &trans, &fcount);
	printf("face   = %s\n", face ? face : "(null)");
	printf("ndigit = %d\n", ndigit);
	printf("trans  = %d\n", trans);
	printf("fcount = %d\n", fcount);
    }
    printf("</pre>\n");

    if (face) free(face);
    return 0;
}

int main()
{
    global_config = init_config();
    if (!global_config) {
        fprintf(stderr, "Failed to initialize configuration\n");
        return 1;
    }

    char *request_method = getenv("REQUEST_METHOD");
    char *path_info = getenv("PATH_INFO");

    /* Seed random number generator */
    srand(time(NULL));

    /* Check if this is a debug request */
    if (path_info && strstr(path_info, "debug")) {
        cntr_debug_handler(global_config);
    }
    else {
        /* Handle counter display */
        cntr_results counter;
        counter.count = 0;
        counter.date = 0;

        if (!global_config->cntr_file || !*global_config->cntr_file) {
            printf("Content-Type: text/plain\r\n\r\n");
            printf("Error: No counter file configured\n");
            cleanup_config(global_config);
            return 1;
        }

        if (path_info && strlen(path_info)) {
            cntr_lookup(global_config, path_info, &counter);
        }

        cntr_draw_digit(global_config, counter.count);
    }

    cleanup_config(global_config);
    return 0;
}
