#ifndef KEYVALUE_H
#define KEYVALUE_H

#include <time.h>

#ifdef HAVE_GDBM
#include <gdbm.h>
#define DBM_FILE GDBM_FILE
#define dbm_fetch gdbm_fetch
#define dbm_store gdbm_store
#define dbm_close gdbm_close
#define DBM_REPLACE GDBM_REPLACE
#else
#include <ndbm.h>
#define DBM_FILE DBM *
#endif

typedef struct {
    unsigned long count;
    time_t date;
} cntr_results;

typedef struct {
    int cntr_auto_add;
    char *cntr_file;
    char *cntr_timefmt;
    char *cntr_facedir;
} cntr_config_rec;

DBM_FILE do_dbm_open(char *pcntr_file, char *err);
void do_dbm_close(DBM_FILE dbm);
char *cntr_incdbm(cntr_results *results, cntr_config_rec *c, char *uri);
char *cntr_inc(cntr_results *results, cntr_config_rec *c, char *uri);
int cntr_lookup(cntr_config_rec *c, const char *uri, cntr_results *counter);

#endif
