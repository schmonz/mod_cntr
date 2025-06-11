#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <gdbm.h>

#include "keyvalue.h"

extern char *ap_pstrdup(const char *s);

/* DBM-specific handle structure */
struct kvstore_handle {
    GDBM_FILE dbm;
    kvstore_mode_t mode;
    char *path;
};

/* DBM implementation functions */
static kvstore_handle_t* dbm_open(const char *path, kvstore_mode_t mode, kvstore_error_t *error)
{
    kvstore_handle_t *handle = calloc(1, sizeof(kvstore_handle_t));
    if (!handle) {
        *error = KVSTORE_ERROR_MEMORY;
        return NULL;
    }

    int gdbm_mode;
    int file_mode;

    switch (mode) {
        case KVSTORE_MODE_READ_ONLY:
            gdbm_mode = GDBM_READER;
            file_mode = 0444;
            break;
        case KVSTORE_MODE_READ_WRITE:
            gdbm_mode = GDBM_WRITER;
            file_mode = 0666;
            break;
        case KVSTORE_MODE_CREATE:
            gdbm_mode = GDBM_WRCREAT;
            file_mode = 0666;
            break;
        default:
            free(handle);
            *error = KVSTORE_ERROR_INVALID_PARAM;
            return NULL;
    }

    handle->dbm = gdbm_open((char*)path, 512, gdbm_mode, file_mode, 0);
    if (!handle->dbm) {
        free(handle);
        *error = KVSTORE_ERROR_OPEN;
        return NULL;
    }

    handle->mode = mode;
    handle->path = ap_pstrdup(path);
    *error = KVSTORE_OK;
    return handle;
}

static kvstore_error_t dbm_close(kvstore_handle_t *handle)
{
    if (!handle) {
        return KVSTORE_ERROR_INVALID_PARAM;
    }

    if (handle->dbm) {
        gdbm_close(handle->dbm);
    }

    if (handle->path) {
        free(handle->path);
    }

    free(handle);
    return KVSTORE_OK;
}

static kvstore_error_t dbm_get(kvstore_handle_t *handle, const kvstore_key_t *key, kvstore_value_t *value)
{
    if (!handle || !key || !value) {
        return KVSTORE_ERROR_INVALID_PARAM;
    }

    datum dbm_key, dbm_data;
    dbm_key.dptr = key->data;
    dbm_key.dsize = key->size;

    dbm_data = gdbm_fetch(handle->dbm, dbm_key);

    if (!dbm_data.dptr) {
        return KVSTORE_ERROR_NOT_FOUND;
    }

    /* Allocate memory for the value and copy data */
    value->data = malloc(dbm_data.dsize);
    if (!value->data) {
        free(dbm_data.dptr);
        return KVSTORE_ERROR_MEMORY;
    }

    memcpy(value->data, dbm_data.dptr, dbm_data.dsize);
    value->size = dbm_data.dsize;

    free(dbm_data.dptr);
    return KVSTORE_OK;
}

static kvstore_error_t dbm_put(kvstore_handle_t *handle, const kvstore_key_t *key, const kvstore_value_t *value)
{
    if (!handle || !key || !value) {
        return KVSTORE_ERROR_INVALID_PARAM;
    }

    datum dbm_key, dbm_data;
    dbm_key.dptr = key->data;
    dbm_key.dsize = key->size;
    dbm_data.dptr = value->data;
    dbm_data.dsize = value->size;

    int result = gdbm_store(handle->dbm, dbm_key, dbm_data, GDBM_REPLACE);

    return (result == 0) ? KVSTORE_OK : KVSTORE_ERROR_WRITE;
}

static kvstore_error_t dbm_delete(kvstore_handle_t *handle, const kvstore_key_t *key)
{
    if (!handle || !key) {
        return KVSTORE_ERROR_INVALID_PARAM;
    }

    datum dbm_key;
    dbm_key.dptr = key->data;
    dbm_key.dsize = key->size;

    int result = gdbm_delete(handle->dbm, dbm_key);

    return (result == 0) ? KVSTORE_OK : KVSTORE_ERROR_DELETE;
}

static const char* dbm_error_string(kvstore_error_t error)
{
    switch (error) {
        case KVSTORE_OK:
            return "Success";
        case KVSTORE_ERROR_OPEN:
            return "Failed to open key-value store";
        case KVSTORE_ERROR_CLOSE:
            return "Failed to close key-value store";
        case KVSTORE_ERROR_READ:
            return "Failed to read from key-value store";
        case KVSTORE_ERROR_WRITE:
            return "Failed to write to key-value store";
        case KVSTORE_ERROR_DELETE:
            return "Failed to delete from key-value store";
        case KVSTORE_ERROR_NOT_FOUND:
            return "Key not found in key-value store";
        case KVSTORE_ERROR_MEMORY:
            return "Memory allocation error";
        case KVSTORE_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        default:
            return "Unknown error";
    }
}

/* DBM interface instance */
static kvstore_interface_t dbm_interface = {
    .open = dbm_open,
    .close = dbm_close,
    .get = dbm_get,
    .put = dbm_put,
    .delete = dbm_delete,
    .error_string = dbm_error_string
};

/* Public API functions */
kvstore_interface_t* kvstore_get_dbm_interface(void)
{
    return &dbm_interface;
}

kvstore_key_t kvstore_key_from_string(const char *str)
{
    kvstore_key_t key;
    key.size = strlen(str);
    key.data = malloc(key.size);
    if (key.data) {
        memcpy(key.data, str, key.size);
    } else {
        key.size = 0;
    }
    return key;
}

void kvstore_key_free(kvstore_key_t *key)
{
    if (key && key->data) {
        free(key->data);
        key->data = NULL;
        key->size = 0;
    }
}

void kvstore_value_free(kvstore_value_t *value)
{
    if (value && value->data) {
        free(value->data);
        value->data = NULL;
        value->size = 0;
    }
}

/* High-level counter functions using the abstraction */
char *cntr_inc(cntr_results *results, cntr_config_rec *c, const char *uri)
{
    kvstore_interface_t *kv = kvstore_get_dbm_interface();
    kvstore_error_t error;

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

    /* Initialize results */
    results->count = 0;
    results->date = 0;

    /* Open the key-value store */
    kvstore_handle_t *handle = kv->open(c->cntr_file, KVSTORE_MODE_CREATE, &error);
    if (!handle) {
        char *err_msg = ap_pstrdup(kv->error_string(error));
        free(puri);
        return err_msg;
    }

    /* Create key from URI */
    kvstore_key_t key = kvstore_key_from_string(puri);
    if (!key.data) {
        kv->close(handle);
        free(puri);
        return ap_pstrdup("Memory allocation error");
    }

    /* Try to get existing value */
    kvstore_value_t value;
    error = kv->get(handle, &key, &value);

    if (error == KVSTORE_OK) {
        /* Found existing record, increment counter */
        if (value.size == sizeof(cntr_results)) {
            memcpy(results, value.data, sizeof(cntr_results));
            results->count++;
        } else {
            /* Corrupted data, reset */
            results->count = 1;
            results->date = time(0L);
        }
        kvstore_value_free(&value);
    } else if (error == KVSTORE_ERROR_NOT_FOUND) {
        /* Create new record */
        results->count = 1;
        results->date = time(0L);
    } else {
        /* Error occurred */
        char *err_msg = ap_pstrdup(kv->error_string(error));
        kvstore_key_free(&key);
        kv->close(handle);
        free(puri);
        return err_msg;
    }

    /* Store updated/new record if auto_add is enabled or record existed */
    if (error == KVSTORE_OK || c->cntr_auto_add) {
        kvstore_value_t new_value;
        new_value.data = results;
        new_value.size = sizeof(cntr_results);

        error = kv->put(handle, &key, &new_value);
        if (error != KVSTORE_OK) {
            char *err_msg = ap_pstrdup(kv->error_string(error));
            kvstore_key_free(&key);
            kv->close(handle);
            free(puri);
            return err_msg;
        }
    }

    kvstore_key_free(&key);
    kv->close(handle);
    free(puri);
    return NULL;  /* Success */
}

int cntr_lookup(cntr_config_rec *c, const char *uri, cntr_results *counter)
{
    kvstore_interface_t *kv = kvstore_get_dbm_interface();
    kvstore_error_t error;
    int result = 0;

#ifdef DEBUG_CGI
    FILE *dbg = fopen("/dev/tty", "w");
    fprintf(dbg, "cntr_lookup - URI: %s\n", uri);
#endif

    /* Open the key-value store in read-only mode */
    kvstore_handle_t *handle = kv->open(c->cntr_file, KVSTORE_MODE_READ_ONLY, &error);
    if (!handle) {
        fprintf(stderr, "Failed to open %s: %s\n", c->cntr_file, kv->error_string(error));
#ifdef DEBUG_CGI
        fclose(dbg);
#endif
        return 0;
    }

    /* Create key from URI */
    kvstore_key_t key = kvstore_key_from_string(uri);
    if (!key.data) {
        kv->close(handle);
#ifdef DEBUG_CGI
        fclose(dbg);
#endif
        return 0;
    }

    /* Try to get the value */
    kvstore_value_t value;
    error = kv->get(handle, &key, &value);

    if (error == KVSTORE_OK && value.size == sizeof(cntr_results)) {
        memcpy(counter, value.data, sizeof(cntr_results));
        result = counter->count;
        kvstore_value_free(&value);
    }

    kvstore_key_free(&key);
    kv->close(handle);

#ifdef DEBUG_CGI
    fclose(dbg);
#endif

    return result;
}
