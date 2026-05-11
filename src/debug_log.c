#include "debug_log.h"
#include <stdarg.h>
#include <stdio.h>

void Debug_Log(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
