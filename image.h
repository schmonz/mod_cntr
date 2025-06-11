#ifndef IMAGE_H
#define IMAGE_H

#include <gd.h>

#include "keyvalue.h"

#define MAXNDIGIT 12

int cntr_parse_query(char *query, char **face, int *ndigit, int *trans, int *fcount);
gdImagePtr cntr_read_digit(int digit);
int cntr_draw_digit(cntr_config_rec *c, int count);

#endif
