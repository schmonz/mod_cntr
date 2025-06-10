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

                    while( direntry = readdir( facedir ) ) {
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
    } while (q = strtok(NULL, "&"));
    free(qbuf);
    return 0;
}

gdImagePtr cntr_read_digit(int digit)
{
    char file[256];
    gdImagePtr im = NULL;
    FILE *fp;

    sprintf(file, "%d.gif", digit);
    if (fp = fopen(file, "r")) {
        im = gdImageCreateFromGif(fp);
        fclose(fp);
    }
    else {
        fprintf(stderr, "%d.gif: %s\n", digit, strerror(errno));
    }
    return im;
}

int cntr_draw_digit(cntr_config_rec * c, int count)
{
    int i;
    ////int resize = 0;
    int width = 0;
    int height = 0;
    char digitbuf[256], digitfmt[16], *dp;
    gdImagePtr imdigit[10] = {NULL};
    gdImagePtr imgd;
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

    for (dp = digitbuf; dp && *dp; dp++) {
        if (isdigit(*dp)) {
            i = *dp - '0';
            if (imdigit[i] == NULL)
                if ((imdigit[i] = cntr_read_digit(i)) == NULL) {
                    goto cleanup;
                }
            if (gdImageSY(imdigit[i]) > height)
                height = gdImageSY(imdigit[i]);
            width += gdImageSX(imdigit[i]);
        }
    }

    /* Create output image */
    if ((imgd = gdImageCreate(width, height)) == NULL) {
        goto cleanup;
    }

    /* Draw rest of digits */
    width = 0;
    for (dp = digitbuf; dp && *dp; dp++) {
        if (isdigit(*dp)) {
            i = *dp - '0';
            gdImageCopyResized(imgd, imdigit[i], width, 0, 0, 0,
                               imdigit[i]->sx, height,
                               imdigit[i]->sx, imdigit[i]->sy);
            width += gdImageSX(imdigit[i]);
        }
    }

    /* Make output interlaced */
    gdImageInterlace(imgd, 1);
    if (trans) {
        gdImageColorTransparent(imgd, gdImageGetPixel(imgd, 0, 0));
    }
    /* Spit out image */

    gif_data = gdImageGifPtr(imgd, &gif_size);

    printf("Content-Type: image/gif\r\n");
    printf("Pragma: no-cache\r\n");
    printf("Expires: Thursday, 01-Jan-1970 00:00:00\r\n");
    printf("\r\n");
    fwrite(gif_data, gif_size, 1, stdout);

    gdFree(gif_data);
    gdImageDestroy(imgd);
    goto cleanup;

cleanup:
    for (i = 0; i < 10; i++) {
        if (imdigit[i] != NULL) {
            gdImageDestroy(imdigit[i]);
        }
    }

    if (face) free(face);
    return 0;
}
