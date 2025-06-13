#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <png.h>

#include "image.h"

extern cntr_config_rec *global_config;

#define DEFAULT_FACE	"default"

/* =====================================================
 * PNG-SPECIFIC IMPLEMENTATION OF ABSTRACTION LAYER
 * ===================================================== */

/* PNG-specific image structure */
struct cntr_image {
    png_bytep *row_pointers;
    int width;
    int height;
    int color_type;
    int bit_depth;
    png_colorp palette;
    int num_palette;
    int transparent_color;
};

/* Create a default grayscale palette */
static int create_default_palette(cntr_image_t* img) {
    img->num_palette = 256;
    img->palette = malloc(256 * sizeof(png_color));
    if (!img->palette) {
        return -1;
    }

    // Create grayscale palette
    for (int i = 0; i < 256; i++) {
        img->palette[i].red = i;
        img->palette[i].green = i;
        img->palette[i].blue = i;
    }

    return 0;
}

/* PNG Implementation functions */
static cntr_image_t* png_create(int width, int height)
{
    cntr_image_t* img = malloc(sizeof(cntr_image_t));
    if (!img) return NULL;

    img->width = width;
    img->height = height;
    img->color_type = PNG_COLOR_TYPE_PALETTE;
    img->bit_depth = 8;
    img->palette = NULL;
    img->num_palette = 0;
    img->transparent_color = -1;

    // Create default palette
    if (create_default_palette(img) != 0) {
        free(img);
        return NULL;
    }

    // Allocate row pointers
    img->row_pointers = malloc(height * sizeof(png_bytep));
    if (!img->row_pointers) {
        free(img->palette);
        free(img);
        return NULL;
    }

    // Allocate image data (1 byte per pixel for palette mode)
    for (int y = 0; y < height; y++) {
        img->row_pointers[y] = malloc(width * sizeof(png_byte));
        if (!img->row_pointers[y]) {
            // Clean up on failure
            for (int i = 0; i < y; i++) {
                free(img->row_pointers[i]);
            }
            free(img->row_pointers);
            free(img->palette);
            free(img);
            return NULL;
        }
        memset(img->row_pointers[y], 0, width);
    }

    return img;
}

static cntr_image_t* png_load_from_file(const char* filename)
{
    FILE* fp;
    cntr_image_t* img;
    png_structp png_ptr;
    png_infop info_ptr;
    png_byte header[8];

    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        return NULL;
    }

    // Check if it's a PNG file
    size_t ret = fread(header, 1, 8, fp);
    if (ret != 8) {
        fprintf(stderr, "fread() failed: %zu\n", ret);
    }
    if (png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "%s: Not a PNG file\n", filename);
        fclose(fp);
        return NULL;
    }

    // Create PNG structures
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    // Set up error handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }

    // Initialize PNG I/O
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    // Read PNG info
    png_read_info(png_ptr, info_ptr);

    img = malloc(sizeof(cntr_image_t));
    if (!img) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }

    img->width = png_get_image_width(png_ptr, info_ptr);
    img->height = png_get_image_height(png_ptr, info_ptr);
    img->color_type = png_get_color_type(png_ptr, info_ptr);
    img->bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    img->palette = NULL;
    img->num_palette = 0;
    img->transparent_color = -1;

    // Handle different color types and convert to palette if needed
    if (img->color_type == PNG_COLOR_TYPE_RGB) {
        png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1);
        img->color_type = PNG_COLOR_TYPE_GRAY;
    }
    if (img->color_type == PNG_COLOR_TYPE_GRAY && img->bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    if (img->bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }

    // Convert grayscale to palette
    if (img->color_type == PNG_COLOR_TYPE_GRAY) {
        if (create_default_palette(img) != 0) {
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            free(img);
            return NULL;
        }
        img->color_type = PNG_COLOR_TYPE_PALETTE;
    }

    // Get existing palette information if already palette
    if (img->color_type == PNG_COLOR_TYPE_PALETTE) {
        png_colorp file_palette;
        int file_num_palette;
        if (png_get_PLTE(png_ptr, info_ptr, &file_palette, &file_num_palette)) {
            // Copy the palette from file
            img->palette = malloc(file_num_palette * sizeof(png_color));
            if (img->palette) {
                memcpy(img->palette, file_palette, file_num_palette * sizeof(png_color));
                img->num_palette = file_num_palette;
            }
        } else if (!img->palette) {
            // Create default palette if none exists
            if (create_default_palette(img) != 0) {
                png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
                fclose(fp);
                free(img);
                return NULL;
            }
        }
    }

    // Update info after transformations
    png_read_update_info(png_ptr, info_ptr);

    // Allocate row pointers
    img->row_pointers = malloc(img->height * sizeof(png_bytep));
    if (!img->row_pointers) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        if (img->palette) free(img->palette);
        free(img);
        return NULL;
    }

    // Allocate image data
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    for (int y = 0; y < img->height; y++) {
        img->row_pointers[y] = malloc(rowbytes);
        if (!img->row_pointers[y]) {
            for (int i = 0; i < y; i++) {
                free(img->row_pointers[i]);
            }
            free(img->row_pointers);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            if (img->palette) free(img->palette);
            free(img);
            return NULL;
        }
    }

    // Read the image
    png_read_image(png_ptr, img->row_pointers);
    png_read_end(png_ptr, NULL);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    return img;
}

static void png_destroy(cntr_image_t* img)
{
    if (img) {
        if (img->row_pointers) {
            for (int y = 0; y < img->height; y++) {
                if (img->row_pointers[y]) {
                    free(img->row_pointers[y]);
                }
            }
            free(img->row_pointers);
        }
        if (img->palette) {
            free(img->palette);
        }
        free(img);
    }
}

static int png_get_width(cntr_image_t* img)
{
    if (!img) return 0;
    return img->width;
}

static int png_get_height(cntr_image_t* img)
{
    if (!img) return 0;
    return img->height;
}

static void png_copy_resized(cntr_image_t* dst, cntr_image_t* src,
                           int dst_x, int dst_y, int src_x, int src_y,
                           int dst_w, int dst_h, int src_w, int src_h)
{
    if (!dst || !src || !dst->row_pointers || !src->row_pointers) return;

    // Simple nearest-neighbor scaling
    for (int y = 0; y < dst_h; y++) {
        if (dst_y + y >= dst->height) break;

        int src_y_scaled = src_y + (y * src_h) / dst_h;
        if (src_y_scaled >= src->height) src_y_scaled = src->height - 1;

        for (int x = 0; x < dst_w; x++) {
            if (dst_x + x >= dst->width) break;

            int src_x_scaled = src_x + (x * src_w) / dst_w;
            if (src_x_scaled >= src->width) src_x_scaled = src->width - 1;

            dst->row_pointers[dst_y + y][dst_x + x] =
                src->row_pointers[src_y_scaled][src_x_scaled];
        }
    }
}

static void png_set_interlaced(cntr_image_t* img, int interlaced)
{
    // PNG interlacing is set during write, not on the image structure
    // This function is kept for compatibility but doesn't do anything
    (void)img;
    (void)interlaced;
}

static void png_set_transparent(cntr_image_t* img, int x, int y)
{
    if (!img || !img->row_pointers) return;
    if (x >= img->width || y >= img->height) return;

    img->transparent_color = img->row_pointers[y][x];
}

/* Memory buffer structure for PNG writing */
typedef struct {
    unsigned char *data;
    size_t size;
    size_t pos;
} png_write_buffer_t;

/* PNG write callback function */
static void png_write_data_callback(png_structp png_ptr, png_bytep data, png_size_t length)
{
    png_write_buffer_t *buffer = (png_write_buffer_t*)png_get_io_ptr(png_ptr);

    if (buffer->pos + length > buffer->size) {
        size_t new_size = buffer->size * 2;
        while (new_size < buffer->pos + length) {
            new_size *= 2;
        }
        unsigned char *new_data = realloc(buffer->data, new_size);
        if (!new_data) {
            png_error(png_ptr, "Write Error: Out of memory");
            return;
        }
        buffer->data = new_data;
        buffer->size = new_size;
    }

    memcpy(buffer->data + buffer->pos, data, length);
    buffer->pos += length;
}

static void png_flush_callback(png_structp png_ptr)
{
    // Nothing to do for memory buffer
    (void)png_ptr;
}

static void* png_get_gif_data(cntr_image_t* img, int* size)
{
    if (!img || !size) return NULL;

    // Ensure we have a valid palette
    if (img->color_type == PNG_COLOR_TYPE_PALETTE && (!img->palette || img->num_palette == 0)) {
        if (create_default_palette(img) != 0) {
            return NULL;
        }
    }

    png_structp png_ptr;
    png_infop info_ptr;
    png_write_buffer_t buffer;

    // Initialize buffer
    buffer.data = malloc(8192); // Start with 8KB
    if (!buffer.data) return NULL;
    buffer.size = 8192;
    buffer.pos = 0;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        free(buffer.data);
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        free(buffer.data);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        free(buffer.data);
        return NULL;
    }

    // Set up memory writing
    png_set_write_fn(png_ptr, &buffer, png_write_data_callback, png_flush_callback);

    // Set PNG header
    png_set_IHDR(png_ptr, info_ptr, img->width, img->height,
                 8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    // Set palette - this is crucial!
    if (img->palette && img->num_palette > 0) {
        png_set_PLTE(png_ptr, info_ptr, img->palette, img->num_palette);
    }

    // Set transparency
    if (img->transparent_color >= 0 && img->transparent_color < img->num_palette) {
        png_byte trans_values[256];
        for (int i = 0; i < img->num_palette; i++) {
            trans_values[i] = (i == img->transparent_color) ? 0 : 255;
        }
        png_set_tRNS(png_ptr, info_ptr, trans_values, img->num_palette, NULL);
    }

    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, img->row_pointers);
    png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    *size = buffer.pos;
    return buffer.data;
}

static void png_free_png_data(void* data)
{
    if (data) {
        free(data);
    }
}

/* PNG operations table */
static cntr_image_ops_t png_ops = {
    .create = png_create,
    .load_from_file = png_load_from_file,
    .destroy = png_destroy,
    .get_width = png_get_width,
    .get_height = png_get_height,
    .copy_resized = png_copy_resized,
    .set_interlaced = png_set_interlaced,
    .set_transparent = png_set_transparent,
    .get_gif_data = png_get_gif_data,
    .free_data = png_free_png_data
};

/* Global operations pointer */
cntr_image_ops_t* cntr_image_ops = NULL;

/* Initialize with PNG implementation */
static int cntr_image_init_png(void)
{
    cntr_image_ops = &png_ops;
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
    /* Initialize with PNG implementation */
    return cntr_image_init_png();
}

int cntr_parse_query(
    char *query, char **face, int *ndigit, int *trans, int *fcount)
{
    char *qbuf, *q;

    if (!query || !*query) {
        return 0;
    }

    qbuf = strdup(query);
    q = strtok(qbuf, "&");

    do {
        if (strncasecmp(q, "face=", 5) == 0) {
            if (*face) {
                free(*face);
                *face = NULL;
            }

            *face = strdup(q + 5);

            /******************************************
             *                                        *
             * 'Ere we go !!!!!! (handle "random")    *
             *                                        *
             ******************************************/
            if( !strcasecmp(*face, "random") ) {
                static int rand_initialized = 0;
                if (!rand_initialized) {
                    unsigned int seed;
                    FILE *urandom = fopen("/dev/urandom", "r");
                    if (urandom) {
                        fread(&seed, sizeof(seed), 1, urandom);
                        fclose(urandom);
                    } else {
                        /* Fallback to a mix of time and process ID if /dev/urandom isn't available */
                        seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
                    }
                    srand(seed);
                    rand_initialized = 1;
                }

                DIR *facedir = opendir( global_config->cntr_facedir );
                if( facedir ) {
                    struct dirent *direntry;
                    char *faces[256];
                    int face_count = 0;

                    while(( direntry = readdir( facedir ) )) {
                        if( *(direntry->d_name) == '.' )
                            continue;
                        if(face_count < 256) {
                            faces[face_count++] = strdup(direntry->d_name);
                        }
                    } /* wend */
                    closedir(facedir);

                    if(face_count > 0) {
                        free(*face);
                        *face = strdup(faces[rand() % face_count]);

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

    sprintf(file, "%d.png", digit);
    img = cntr_image_load_from_file(file);

    if (!img) {
        fprintf(stderr, "%d.png: %s\n", digit, strerror(errno));
    }

    return img;
}

int cntr_draw_digit(cntr_config_rec * c, int count)
{
    int i;
    int width = 0;
    int height = 0;
    char digitbuf[256], *dp;
    cntr_image_t* imdigit[10] = {NULL};
    cntr_image_t* imgd = NULL;
    void *png_data;
    int png_size;
    char *face = NULL;
    int ndigit = 0;
    int trans = 0;
    int fcount = 0;

    char *query_string = getenv("QUERY_STRING");

    if (query_string) {
        cntr_parse_query(query_string, &face, &ndigit, &trans, &fcount);
    }

    if (ndigit > 0) {
        sprintf(digitbuf, "%0*d", (ndigit <= MAXNDIGIT ? ndigit : MAXNDIGIT), (fcount ? fcount : count));
    }
    else {
        sprintf(digitbuf, "%d", (fcount ? fcount : count));
    }

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
        if (isdigit((int)*dp)) {
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
        if (isdigit((int)*dp)) {
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
    png_data = cntr_image_get_gif_data(imgd, &png_size);
    if (png_data) {
        printf("Content-Type: image/png\r\n");
        printf("Pragma: no-cache\r\n");
        printf("Expires: Thursday, 01-Jan-1970 00:00:00\r\n");
        printf("\r\n");
        fwrite(png_data, png_size, 1, stdout);

        cntr_image_free_data(png_data);
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
