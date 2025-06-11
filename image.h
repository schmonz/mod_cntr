#ifndef IMAGE_H
#define IMAGE_H

#include "keyvalue.h"

#define MAXNDIGIT 12

/* Forward declaration - implementation is hidden */
typedef struct cntr_image cntr_image_t;

/* Image operations interface */
typedef struct {
    /* Create/destroy operations */
    cntr_image_t* (*create)(int width, int height);
    cntr_image_t* (*load_from_file)(const char* filename);
    void (*destroy)(cntr_image_t* img);

    /* Image property operations */
    int (*get_width)(cntr_image_t* img);
    int (*get_height)(cntr_image_t* img);

    /* Drawing operations */
    void (*copy_resized)(cntr_image_t* dst, cntr_image_t* src,
                        int dst_x, int dst_y, int src_x, int src_y,
                        int dst_w, int dst_h, int src_w, int src_h);

    /* Image effects */
    void (*set_interlaced)(cntr_image_t* img, int interlaced);
    void (*set_transparent)(cntr_image_t* img, int x, int y);

    /* Output operations */
    void* (*get_gif_data)(cntr_image_t* img, int* size);
    void (*free_data)(void* data);

} cntr_image_ops_t;

/* Global image operations - will be set to specific implementation */
extern cntr_image_ops_t* cntr_image_ops;

/* Initialize the image system with a specific implementation */
int cntr_image_init(cntr_image_ops_t* ops);
void cntr_image_cleanup(void);

/* Convenience macros for easier usage */
#define cntr_image_create(w, h) cntr_image_ops->create(w, h)
#define cntr_image_load_from_file(f) cntr_image_ops->load_from_file(f)
#define cntr_image_destroy(img) cntr_image_ops->destroy(img)
#define cntr_image_get_width(img) cntr_image_ops->get_width(img)
#define cntr_image_get_height(img) cntr_image_ops->get_height(img)
#define cntr_image_copy_resized(dst, src, dx, dy, sx, sy, dw, dh, sw, sh) \
    cntr_image_ops->copy_resized(dst, src, dx, dy, sx, sy, dw, dh, sw, sh)
#define cntr_image_set_interlaced(img, i) cntr_image_ops->set_interlaced(img, i)
#define cntr_image_set_transparent(img, x, y) cntr_image_ops->set_transparent(img, x, y)
#define cntr_image_get_gif_data(img, size) cntr_image_ops->get_gif_data(img, size)
#define cntr_image_free_data(data) cntr_image_ops->free_data(data)

/* Query parsing - unchanged */
int cntr_parse_query(char *query, char **face, int *ndigit, int *trans, int *fcount);

/* Image operations using abstraction layer */
cntr_image_t* cntr_read_digit(int digit);
int cntr_draw_digit(cntr_config_rec *c, int count);

/* Initialize image system - call this before using image functions */
int cntr_image_system_init(void);

#endif
