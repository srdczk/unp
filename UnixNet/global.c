#include "global.h"

int error_handle(const char *s) {
    perror(s);
    exit(-1);
}