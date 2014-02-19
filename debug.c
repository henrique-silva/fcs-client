//============================================================================
// Author      : Lucas Russo
// Version     : 1.0
// Description : Common definitons for supporting different boards
//============================================================================

#include "debug.h"
#include <stdio.h>

void debug_print(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

