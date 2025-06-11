#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <gd.h>
#include "image.h"

extern char *ap_pstrdup(const char *s);
extern cntr_config_rec *global_config;

#define DEFAULT_FACE	"default"

/* =====================================================
 * GD-SPECIFIC IMPLEMENTATION OF ABSTRACTION LAYER
 * ===================================================== */

/* GD-specific image structure */
struct cntr_image {
    gdImagePtr gd_image;
};

/* GD Implementation functions */
static cntr_image_t* gd_create(int width, int height)
{
    cntr_image_t* img = malloc(sizeof(cntr_image_t));
    if (!img) return NULL;

    img->gd_image = gdImageCreate(width, height);
    if (!img->gd_image) {
        free(img);
        return NULL;
    }

    return img;
}

static cntr_image_t* gd_load_from_file(const char* filename)
{
    FILE* fp;
    cntr_image_t* img;

    fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        return NULL;
    }

    img = malloc(sizeof(cntr_image_t));
    if (!img) {
        fclose(fp);
        return NULL;
    }

    img->gd_image = gdImageCreateFromGif(fp);
    fclose(fp);

    if (!img->gd_image) {
        free(img);
        return NULL;
    }

    return img;
}

static void gd_destroy(cntr_image_t* img)
{
    if (img) {
        if (img->gd_image) {
            gdImageDestroy(img->gd_image);
        }
        free(img);
    }
}

static int gd_get_width(cntr_image_t* img)
{
    if (!img || !img->gd_image) return 0;
    return gdImageSX(img->gd_image);
}

static int gd_get_height(cntr_image_t* img)
{
    if (!img || !img->gd_image) return 0;
    return gdImageSY(img->gd_image);
}

static void gd_copy_resized(cntr_image_t* dst, cntr_image_t* src,
                           int dst_x, int dst_y, int src_x, int src_y,
                           int dst_w, int dst_h, int src_w, int src_h)
{
    if (!dst || !src || !dst->gd_image || !src->gd_image) return;

    gdImageCopyResized(dst->gd_image, src->gd_image,
                      dst_x, dst_y, src_x, src_y,
                      dst_w, dst_h, src_w, src_h);
}

static void gd_set_interlaced(cntr_image_t* img, int interlaced)
{
    if (!img || !img->gd_image) return;
    gdImageInterlace(img->gd_image, interlaced);
}

static void gd_set_transparent(cntr_image_t* img, int x, int y)
{
    if (!img || !img->gd_image) return;
    int color = gdImageGetPixel(img->gd_image, x, y);
    gdImageColorTransparent(img->gd_image, color);
}

static void* gd_get_gif_data(cntr_image_t* img, int* size)
{
    if (!img || !img->gd_image || !size) return NULL;
    return gdImageGifPtr(img->gd_image, size);
}

static void gd_free_data(void* data)
{
    if (data) {
        gdFree(data);
    }
}

/* GD operations table */
static cntr_image_ops_t gd_ops = {
    .create = gd_create,
    .load_from_file = gd_load_from_file,
    .destroy = gd_destroy,
    .get_width = gd_get_width,
    .get_height = gd_get_height,
    .copy_resized = gd_copy_resized,
    .set_interlaced = gd_set_interlaced,
    .set_transparent = gd_set_transparent,
    .get_gif_data = gd_get_gif_data,
    .free_data = gd_free_data
};

/* Global operations pointer */
cntr_image_ops_t* cntr_image_ops = NULL;

/* Initialize with GD implementation */
static int cntr_image_init_gd(void)
{
    cntr_image_ops = &gd_ops;
    return 0;
}

/* Generic initialization function */
int cntr_image_init(cntr_image_ops_t* ops)
{
    if (!ops) return -1;
    cntr_image_ops = ops;
    return 0;
}

void cntr_image_cleanup(void)
{
    cntr_image_ops = NULL;
}

/* =====================================================
 * PUBLIC INTERFACE FUNCTIONS
 * ===================================================== */

int cntr_image_system_init(void)
{
    /* Initialize with GD implementation */
    return cntr_image_init_gd();
}

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
            if( !strcasecmp(*face, "random") ) {
                DIR *facedir = opendir( global_config->cntr_facedir );
                if( facedir ) {
                    struct dirent *direntry;
                    char *faces[256];
                    int face_count = 0;

                    while(( direntry = readdir( facedir ) )) {
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
    } while ((q = strtok(NULL, "&")));
    free(qbuf);
    return 0;
}

cntr_image_t* cntr_read_digit(int digit)
{
    char file[256];
    cntr_image_t* img = NULL;

    sprintf(file, "%d.gif", digit);
    img = cntr_image_load_from_file(file);

    if (!img) {
        fprintf(stderr, "%d.gif: %s\n", digit, strerror(errno));
    }

    return img;
}

int cntr_draw_digit(cntr_config_rec * c, int count)
{
    int i;
    int width = 0;
    int height = 0;
    char digitbuf[256], digitfmt[16], *dp;
    cntr_image_t* imdigit[10] = {NULL};
    cntr_image_t* imgd = NULL;
    void *gif_data;
    int gif_size;
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

    /* Load digit images and calculate dimensions */
    for (dp = digitbuf; dp && *dp; dp++) {
        if (isdigit(*dp)) {
            i = *dp - '0';
            if (imdigit[i] == NULL) {
                if ((imdigit[i] = cntr_read_digit(i)) == NULL) {
                    goto cleanup;
                }
            }
            if (cntr_image_get_height(imdigit[i]) > height)
                height = cntr_image_get_height(imdigit[i]);
            width += cntr_image_get_width(imdigit[i]);
        }
    }

    /* Create output image */
    if ((imgd = cntr_image_create(width, height)) == NULL) {
        goto cleanup;
    }

    /* Draw digits */
    width = 0;
    for (dp = digitbuf; dp && *dp; dp++) {
        if (isdigit(*dp)) {
            i = *dp - '0';
            cntr_image_copy_resized(imgd, imdigit[i], width, 0, 0, 0,
                                   cntr_image_get_width(imdigit[i]), height,
                                   cntr_image_get_width(imdigit[i]),
                                   cntr_image_get_height(imdigit[i]));
            width += cntr_image_get_width(imdigit[i]);
        }
    }

    /* Apply image effects */
    cntr_image_set_interlaced(imgd, 1);
    if (trans) {
        cntr_image_set_transparent(imgd, 0, 0);
    }

    /* Output image */
    gif_data = cntr_image_get_gif_data(imgd, &gif_size);
    if (gif_data) {
        printf("Content-Type: image/gif\r\n");
        printf("Pragma: no-cache\r\n");
        printf("Expires: Thursday, 01-Jan-1970 00:00:00\r\n");
        printf("\r\n");
        fwrite(gif_data, gif_size, 1, stdout);

        cntr_image_free_data(gif_data);
    }

cleanup:
    /* Clean up digit images */
    for (i = 0; i < 10; i++) {
        if (imdigit[i] != NULL) {
            cntr_image_destroy(imdigit[i]);
        }
    }

    /* Clean up output image */
    if (imgd) {
        cntr_image_destroy(imgd);
    }

    if (face) free(face);
    return 0;
}
