//============================================================================
// Author      : Lucas Russo
// Version     : 1.0
// Description : Common definitons for supporting different boards
//============================================================================

#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>

void debug_print (const char *fmt, ...) __attribute__((format(printf,1,2)));

// Debug print
#ifdef DEBUG
#define dbg_print(fmt, ...) \
    debug_print(fmt, ## __VA_ARGS__)
#else
#define dbg_print(fmt, ...)
#endif /* Debug print */

#ifdef DEBUG
#define DEBUGP(fmt, ...)                \
    do{                                 \
        dbg_print(fmt, ## __VA_ARGS__); \
    } while(0)
#else
#define DEBUGP(fmt, ...)
#endif

#endif
