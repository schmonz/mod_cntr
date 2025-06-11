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
GDBM_FILE do_dbm_open(char *pcntr_file, char *err)
{
    GDBM_FILE dbm;
    if ((dbm = gdbm_open(pcntr_file, 512, GDBM_WRCREAT, 0666, 0)) == NULL) {
        sprintf(err, "Failed to open counter dbmfile: %s",
                gdbm_strerror(gdbm_errno));
    }
    return dbm;
}

void do_dbm_close(GDBM_FILE dbm)
{
    /* unlocking is automatically taken care of */
    gdbm_close(dbm);
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
    GDBM_FILE dbm;
    char err[256];
    datum d, q;

    results->count = 0;
    results->date = 0;

    q.dptr = uri;
    q.dsize = strlen(q.dptr);

    if ((dbm = do_dbm_open(c->cntr_file, err)) == NULL) {
        return ap_pstrdup(err);
    }

    d = gdbm_fetch(dbm, q);

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
        gdbm_store(dbm, q, d, GDBM_REPLACE);
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
            while ((*q = *(q + 1)))
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
    GDBM_FILE dbm;
    datum d, q;

#ifdef DEBUG_CGI
    FILE *dbg = fopen("/dev/tty", "w");
    fprintf( dbg, "cntr_lookup - URI: %s\n", uri );
#endif

    q.dptr = (char *) uri;
    q.dsize = strlen(q.dptr);

    /*
     * No locking is necessary for read_only. so no do_dbm_open is done.
     */
    if ((dbm = gdbm_open(c->cntr_file, 512, GDBM_READER, 0444, 0)) == NULL) {
        fprintf(stderr, "Failed to open %s\n", c->cntr_file);
#ifdef DEBUG_CGI
        fclose( dbg );
#endif
        return 0;
    }
    d = gdbm_fetch(dbm, q);

    if (d.dptr) {		/* found */
        memcpy(counter, d.dptr, sizeof(cntr_results));
        result = counter->count;
    }

#ifdef DEBUG_CGI
    fclose( dbg );
#endif
    gdbm_close(dbm);
    return result;
}
