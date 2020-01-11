#ifndef     __TURBOSTROI_AUXILIARY_H__
#define    __TURBOSTROI_AUXILIARY_H__

#define  TURBOSTROI_DEBUG_MESSAGES

#define   TURBO_COLOR_BLACK       "\u001b[30m"
#define   TURBO_COLOR_RED           "\u001b[31m"
#define   TURBO_COLOR_GREEN       "\u001b[32m"
#define   TURBO_COLOR_YELLOW     "\u001b[33m"
#define   TURBO_COLOR_BLUE          "\u001b[34m"
#define   TURBO_COLOR_MAGENTA   "\u001b[35m"
#define   TURBO_COLOR_CYAN          "\u001b[36m"
#define   TURBO_COLOR_WHITE        "\u001b[37m"
#define   TURBO_COLOR_RESET        "\u001b[0m"

    
#undef Plat_FloatTime

void posix_death_signal(int signum);
double ElapsedTime ();


void console_print(const char *color, const char *format, ... );
void console_print_debug(const char *color, const char *format, ... );

#endif //__TURBOSTROI_AUXILIARY_H__
