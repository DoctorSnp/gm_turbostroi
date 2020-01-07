#include <stdarg.h>
#include <stdio.h>
#include "auxiliary.h"

void console_print(const char *color, const char *format, ... )
{
    va_list args;
    va_start(args, format);
    printf("%s", color);
    vprintf(format, args);
    printf("%s", COLOR_RESET);
    fflush(stdout);
    va_end(args);
    
}
