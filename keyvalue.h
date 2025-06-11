#ifndef KEYVALUE_H
#define KEYVALUE_H

#include <stddef.h>
#include <time.h>

/* Forward declaration for opaque handle */
typedef struct kvstore_handle kvstore_handle_t;

/* Key-value pair structure */
typedef struct {
    void *data;
    size_t size;
} kvstore_value_t;

typedef struct {
    char *data;
    size_t size;
} kvstore_key_t;

/* Error codes */
typedef enum {
    KVSTORE_OK = 0,
    KVSTORE_ERROR_OPEN,
    KVSTORE_ERROR_CLOSE,
    KVSTORE_ERROR_READ,
    KVSTORE_ERROR_WRITE,
    KVSTORE_ERROR_DELETE,
    KVSTORE_ERROR_NOT_FOUND,
    KVSTORE_ERROR_MEMORY,
    KVSTORE_ERROR_INVALID_PARAM
} kvstore_error_t;

/* Open modes */
typedef enum {
    KVSTORE_MODE_READ_ONLY,
    KVSTORE_MODE_READ_WRITE,
    KVSTORE_MODE_CREATE
} kvstore_mode_t;

/* Key-value store interface */
typedef struct {
    kvstore_handle_t* (*open)(const char *path, kvstore_mode_t mode, kvstore_error_t *error);
    kvstore_error_t (*close)(kvstore_handle_t *handle);
    kvstore_error_t (*get)(kvstore_handle_t *handle, const kvstore_key_t *key, kvstore_value_t *value);
    kvstore_error_t (*put)(kvstore_handle_t *handle, const kvstore_key_t *key, const kvstore_value_t *value);
    kvstore_error_t (*delete)(kvstore_handle_t *handle, const kvstore_key_t *key);
    const char* (*error_string)(kvstore_error_t error);
} kvstore_interface_t;

/* Application-specific data structures */
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

/* Public API functions */
kvstore_interface_t* kvstore_get_dbm_interface(void);
kvstore_key_t kvstore_key_from_string(const char *str);
void kvstore_key_free(kvstore_key_t *key);
void kvstore_value_free(kvstore_value_t *value);

/* High-level counter functions using the abstraction */
char *cntr_inc(cntr_results *results, cntr_config_rec *c, const char *uri);
int cntr_lookup(cntr_config_rec *c, const char *uri, cntr_results *counter);

#endif
