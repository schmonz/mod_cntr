#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "keyvalue.h"

extern char *ap_pstrdup(const char *s);

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
    else {
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
 char*        cntr_incfile( pool* p, cntr_results* results,
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
