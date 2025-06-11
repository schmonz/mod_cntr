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
#include <time.h>

#include "keyvalue.h"
#include "image.h"
#include "roman.h"

/////#define DEBUG_CGI

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

/*
 * Handle command-line key lookup mode
 */
int handle_cli_lookup(cntr_config_rec *config, const char *key)
{
    cntr_results counter;
    counter.count = 0;
    counter.date = 0;

    if (!config->cntr_file || !*config->cntr_file) {
        fprintf(stderr, "Error: No counter file configured\n");
        return 1;
    }

    /* Look up the key in the backing storage */
    cntr_lookup(config, key, &counter);

    /* Print the stored value (count) */
    printf("%ld\n", counter.count);
    return 0;
}

int main(int argc, char *argv[])
{
    global_config = init_config();
    if (!global_config) {
        fprintf(stderr, "Failed to initialize configuration\n");
        return 1;
    }

    /* Check if we're in CLI mode (command-line argument provided) */
    if (argc > 1) {
        /* CLI mode: lookup key and print value */
        int result = handle_cli_lookup(global_config, argv[1]);
        cleanup_config(global_config);
        return result;
    }

    /* CGI mode: original functionality */

    /* Initialize image system before using any image functions */
    if (cntr_image_system_init() != 0) {
        fprintf(stderr, "Failed to initialize image system\n");
        cleanup_config(global_config);
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
            cntr_image_cleanup();
            return 1;
        }

        if (path_info && strlen(path_info)) {
            /* Use the abstracted counter increment function */
            char *error_msg = cntr_inc(&counter, global_config, path_info);
            if (error_msg) {
                printf("Content-Type: text/plain\r\n\r\n");
                printf("Error: %s\n", error_msg);
                free(error_msg);
                cleanup_config(global_config);
                cntr_image_cleanup();
                return 1;
            }
            cntr_lookup(global_config, path_info, &counter);
        }

        cntr_draw_digit(global_config, counter.count);
    }

    cleanup_config(global_config);
    cntr_image_cleanup();
    return 0;
}
