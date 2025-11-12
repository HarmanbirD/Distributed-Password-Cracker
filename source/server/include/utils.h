#ifndef UTILS_H
#define UTILS_H

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int string_to_int(const char *str, int *out);

#endif // UTILS_H
