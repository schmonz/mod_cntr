#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sqlite3.h>
#include "keyvalue.h"

/* SQLite-specific handle structure */
struct kvstore_handle {
    sqlite3 *db;
    kvstore_mode_t mode;
    char *path;
};

/* Initialize the database schema */
static int sqlite_init_schema(sqlite3 *db)
{
    const char *create_table_sql = 
        "CREATE TABLE IF NOT EXISTS kvstore ("
        "key TEXT PRIMARY KEY, "
        "value BLOB"
        ");";
    
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg);
    
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        return -1;
    }
    
    return 0;
}

/* SQLite implementation functions */
static kvstore_handle_t* sqlite_open(const char *path, kvstore_mode_t mode, kvstore_error_t *error)
{
    kvstore_handle_t *handle = calloc(1, sizeof(kvstore_handle_t));
    if (!handle) {
        *error = KVSTORE_ERROR_MEMORY;
        return NULL;
    }

    int sqlite_flags;
    
    switch (mode) {
        case KVSTORE_MODE_READ_ONLY:
            sqlite_flags = SQLITE_OPEN_READONLY;
            break;
        case KVSTORE_MODE_READ_WRITE:
            sqlite_flags = SQLITE_OPEN_READWRITE;
            break;
        case KVSTORE_MODE_CREATE:
            sqlite_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            break;
        default:
            free(handle);
            *error = KVSTORE_ERROR_INVALID_PARAM;
            return NULL;
    }

    int rc = sqlite3_open_v2(path, &handle->db, sqlite_flags, NULL);
    if (rc != SQLITE_OK) {
        free(handle);
        *error = KVSTORE_ERROR_OPEN;
        return NULL;
    }

    /* Initialize schema for create/write modes */
    if (mode != KVSTORE_MODE_READ_ONLY) {
        if (sqlite_init_schema(handle->db) != 0) {
            sqlite3_close(handle->db);
            free(handle);
            *error = KVSTORE_ERROR_OPEN;
            return NULL;
        }
    }

    handle->mode = mode;
    handle->path = strdup(path);
    *error = KVSTORE_OK;
    return handle;
}

static kvstore_error_t sqlite_close(kvstore_handle_t *handle)
{
    if (!handle) {
        return KVSTORE_ERROR_INVALID_PARAM;
    }

    if (handle->db) {
        sqlite3_close(handle->db);
    }
    
    if (handle->path) {
        free(handle->path);
    }
    
    free(handle);
    return KVSTORE_OK;
}

static kvstore_error_t sqlite_get(kvstore_handle_t *handle, const kvstore_key_t *key, kvstore_value_t *value)
{
    if (!handle || !key || !value) {
        return KVSTORE_ERROR_INVALID_PARAM;
    }

    const char *sql = "SELECT value FROM kvstore WHERE key = ?";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(handle->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return KVSTORE_ERROR_READ;
    }

    /* Bind the key as a blob to handle any binary data */
    rc = sqlite3_bind_blob(stmt, 1, key->data, key->size, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return KVSTORE_ERROR_READ;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        /* Found the key, get the value */
        const void *blob_data = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);
        
        value->data = malloc(blob_size);
        if (!value->data) {
            sqlite3_finalize(stmt);
            return KVSTORE_ERROR_MEMORY;
        }
        
        memcpy(value->data, blob_data, blob_size);
        value->size = blob_size;
        
        sqlite3_finalize(stmt);
        return KVSTORE_OK;
    } else if (rc == SQLITE_DONE) {
        /* Key not found */
        sqlite3_finalize(stmt);
        return KVSTORE_ERROR_NOT_FOUND;
    } else {
        /* Error occurred */
        sqlite3_finalize(stmt);
        return KVSTORE_ERROR_READ;
    }
}

static kvstore_error_t sqlite_put(kvstore_handle_t *handle, const kvstore_key_t *key, const kvstore_value_t *value)
{
    if (!handle || !key || !value) {
        return KVSTORE_ERROR_INVALID_PARAM;
    }

    const char *sql = "INSERT OR REPLACE INTO kvstore (key, value) VALUES (?, ?)";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(handle->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return KVSTORE_ERROR_WRITE;
    }

    /* Bind key and value as blobs */
    rc = sqlite3_bind_blob(stmt, 1, key->data, key->size, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return KVSTORE_ERROR_WRITE;
    }

    rc = sqlite3_bind_blob(stmt, 2, value->data, value->size, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return KVSTORE_ERROR_WRITE;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? KVSTORE_OK : KVSTORE_ERROR_WRITE;
}

static kvstore_error_t sqlite_delete(kvstore_handle_t *handle, const kvstore_key_t *key)
{
    if (!handle || !key) {
        return KVSTORE_ERROR_INVALID_PARAM;
    }

    const char *sql = "DELETE FROM kvstore WHERE key = ?";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(handle->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return KVSTORE_ERROR_DELETE;
    }

    rc = sqlite3_bind_blob(stmt, 1, key->data, key->size, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return KVSTORE_ERROR_DELETE;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? KVSTORE_OK : KVSTORE_ERROR_DELETE;
}

static const char* sqlite_error_string(kvstore_error_t error)
{
    switch (error) {
        case KVSTORE_OK:
            return "Success";
        case KVSTORE_ERROR_OPEN:
            return "Failed to open SQLite database";
        case KVSTORE_ERROR_CLOSE:
            return "Failed to close SQLite database";
        case KVSTORE_ERROR_READ:
            return "Failed to read from SQLite database";
        case KVSTORE_ERROR_WRITE:
            return "Failed to write to SQLite database";
        case KVSTORE_ERROR_DELETE:
            return "Failed to delete from SQLite database";
        case KVSTORE_ERROR_NOT_FOUND:
            return "Key not found in SQLite database";
        case KVSTORE_ERROR_MEMORY:
            return "Memory allocation error";
        case KVSTORE_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        default:
            return "Unknown SQLite error";
    }
}

/* SQLite interface instance */
static kvstore_interface_t sqlite_interface = {
    .open = sqlite_open,
    .close = sqlite_close,
    .get = sqlite_get,
    .put = sqlite_put,
    .delete = sqlite_delete,
    .error_string = sqlite_error_string
};

/* Public API functions */
kvstore_interface_t* kvstore_get_sqlite_interface(void)
{
    return &sqlite_interface;
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

/* Helper function to normalize URI by removing double slashes */
static char* normalize_uri(const char *uri)
{
    char *puri = strdup(uri);
    if (!puri) {
        return NULL;
    }

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
    return puri;
}

/* Helper function to handle common kvstore operations setup */
static char* kvstore_setup(cntr_config_rec *c, const char *uri, 
                          kvstore_interface_t **kv_out, 
                          kvstore_handle_t **handle_out, 
                          kvstore_key_t *key_out, 
                          char **puri_out)
{
    *kv_out = kvstore_get_sqlite_interface();
    kvstore_error_t error;
    
    /* Normalize the URI */
    *puri_out = normalize_uri(uri);
    if (!*puri_out) {
        return strdup("Memory allocation error");
    }

    /* Open the key-value store */
    *handle_out = (*kv_out)->open(c->cntr_file, KVSTORE_MODE_CREATE, &error);
    if (!*handle_out) {
        char *err_msg = strdup((*kv_out)->error_string(error));
        free(*puri_out);
        return err_msg;
    }

    /* Create key from URI */
    *key_out = kvstore_key_from_string(*puri_out);
    if (!key_out->data) {
        (*kv_out)->close(*handle_out);
        free(*puri_out);
        return strdup("Memory allocation error creating key");
    }

    return NULL; /* Success */
}

/* Helper function to clean up kvstore resources */
static void kvstore_cleanup(kvstore_interface_t *kv, kvstore_handle_t *handle, 
                           kvstore_key_t *key, char *puri)
{
    kvstore_key_free(key);
    kv->close(handle);
    free(puri);
}

/* High-level counter functions using the abstraction */
char *cntr_inc(cntr_results *results, cntr_config_rec *c, const char *uri)
{
    kvstore_interface_t *kv;
    kvstore_handle_t *handle;
    kvstore_key_t key;
    char *puri;
    
    /* Initialize results */
    results->count = 0;
    results->date = 0;

    /* Setup kvstore resources */
    char *setup_error = kvstore_setup(c, uri, &kv, &handle, &key, &puri);
    if (setup_error) {
        return setup_error;
    }

    /* Try to get existing value */
    kvstore_value_t value;
    kvstore_error_t error = kv->get(handle, &key, &value);
    
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
        char *err_msg = strdup(kv->error_string(error));
        kvstore_cleanup(kv, handle, &key, puri);
        return err_msg;
    }

    /* Store updated/new record if auto_add is enabled or record existed */
    if (error == KVSTORE_OK || c->cntr_auto_add) {
        kvstore_value_t new_value;
        new_value.data = results;
        new_value.size = sizeof(cntr_results);
        
        error = kv->put(handle, &key, &new_value);
        if (error != KVSTORE_OK) {
            char *err_msg = strdup(kv->error_string(error));
            kvstore_cleanup(kv, handle, &key, puri);
            return err_msg;
        }
    }

    kvstore_cleanup(kv, handle, &key, puri);
    return NULL;  /* Success */
}

int cntr_lookup(cntr_config_rec *c, const char *uri, cntr_results *counter)
{
    kvstore_interface_t *kv = kvstore_get_sqlite_interface();
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

char *cntr_set(cntr_results *results, cntr_config_rec *c, const char *uri, unsigned long count)
{
    kvstore_interface_t *kv;
    kvstore_handle_t *handle;
    kvstore_key_t key;
    char *puri;

    /* Initialize results with the provided count */
    results->count = count;
    results->date = time(0L);

    /* Setup kvstore resources */
    char *setup_error = kvstore_setup(c, uri, &kv, &handle, &key, &puri);
    if (setup_error) {
        return setup_error;
    }

    /* Store the new record */
    kvstore_value_t new_value;
    new_value.data = results;
    new_value.size = sizeof(cntr_results);

    kvstore_error_t error = kv->put(handle, &key, &new_value);
    if (error != KVSTORE_OK) {
        char *err_msg = strdup(kv->error_string(error));
        kvstore_cleanup(kv, handle, &key, puri);
        return err_msg;
    }

    kvstore_cleanup(kv, handle, &key, puri);
    return NULL;  /* Success */
}