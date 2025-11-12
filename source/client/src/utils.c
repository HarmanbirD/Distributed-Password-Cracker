#include "utils.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int string_to_int(const char *str, int *out, struct fsm_error *err)
{
    char *end;
    errno    = 0;

    long val = strtol(str, &end, 10);

    if (errno != 0)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    if (*end != '\0')
    {
        SET_ERROR(err, "Invalid characters in input.");
        return -1;
    }

    if (val > INT_MAX || val < INT_MIN)
    {
        char error_message[64];
        snprintf(error_message, sizeof(error_message),
                 "Value '%s' out of range for int (%ld).", str, val);
        SET_ERROR(err, error_message);
        return -1;
    }

    *out = (int)val;
    return 0;
}
