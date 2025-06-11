#ifndef IMAGE_H
#define IMAGE_H

#include "keyvalue.h"

#include <png.h>

#define MAXNDIGIT 12

/* Forward declarations */
typedef struct cntr_image cntr_image_t;
typedef struct cntr_config_rec cntr_config_rec_t;

/* Image operations structure - abstraction layer for different image libraries */
typedef struct cntr_image_ops {
    cntr_image_t* (*create)(int width, int height);
    cntr_image_t* (*load_from_file)(const char* filename);
    void (*destroy)(cntr_image_t* img);
    int (*get_width)(cntr_image_t* img);
    int (*get_height)(cntr_image_t* img);
    void (*copy_resized)(cntr_image_t* dst, cntr_image_t* src,
                        int dst_x, int dst_y, int src_x, int src_y,
                        int dst_w, int dst_h, int src_w, int src_h);
    void (*set_interlaced)(cntr_image_t* img, int interlaced);
    void (*set_transparent)(cntr_image_t* img, int x, int y);
    void* (*get_gif_data)(cntr_image_t* img, int* size);  // Note: now returns PNG data
    void (*free_data)(void* data);
} cntr_image_ops_t;

/* Global operations pointer */
extern cntr_image_ops_t* cntr_image_ops;

/* Image system functions */
int cntr_image_system_init(void);
int cntr_image_init(cntr_image_ops_t* ops);
void cntr_image_cleanup(void);

/* Convenience macros for image operations */
#define cntr_image_create(w, h) \
    (cntr_image_ops ? cntr_image_ops->create(w, h) : NULL)

#define cntr_image_load_from_file(filename) \
    (cntr_image_ops ? cntr_image_ops->load_from_file(filename) : NULL)

#define cntr_image_destroy(img) \
    do { if (cntr_image_ops && img) cntr_image_ops->destroy(img); } while(0)

#define cntr_image_get_width(img) \
    (cntr_image_ops && img ? cntr_image_ops->get_width(img) : 0)

#define cntr_image_get_height(img) \
    (cntr_image_ops && img ? cntr_image_ops->get_height(img) : 0)

#define cntr_image_copy_resized(dst, src, dx, dy, sx, sy, dw, dh, sw, sh) \
    do { if (cntr_image_ops) cntr_image_ops->copy_resized(dst, src, dx, dy, sx, sy, dw, dh, sw, sh); } while(0)

#define cntr_image_set_interlaced(img, interlaced) \
    do { if (cntr_image_ops) cntr_image_ops->set_interlaced(img, interlaced); } while(0)

#define cntr_image_set_transparent(img, x, y) \
    do { if (cntr_image_ops) cntr_image_ops->set_transparent(img, x, y); } while(0)

#define cntr_image_get_gif_data(img, size) \
    (cntr_image_ops ? cntr_image_ops->get_gif_data(img, size) : NULL)

#define cntr_image_free_data(data) \
    do { if (cntr_image_ops && data) cntr_image_ops->free_data(data); } while(0)

/* Image utility functions */
int cntr_parse_query(char *query, char **face, int *ndigit, int *trans, int *fcount);
cntr_image_t* cntr_read_digit(int digit);
int cntr_draw_digit(cntr_config_rec *c, int count);

/* Counter functions (implemented in keyvalue.c) */
int cntr_lookup(cntr_config_rec *c, const char *key, cntr_results *results);
char* cntr_inc(cntr_results *results, cntr_config_rec *c, const char *key);

#endif /* IMAGE_H */
