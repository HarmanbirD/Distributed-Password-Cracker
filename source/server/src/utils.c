#include "utils.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int string_to_int(const char *str, int *out)
{
    char *end;
    errno = 0;

    long val = strtol(str, &end, 10);

    if (errno != 0 || *end != '\0' || val > INT_MAX || val < INT_MIN)
        return -1;
    *out = (int)val;
    return 0;
}
