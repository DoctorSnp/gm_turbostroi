#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "auxiliary.h"
#include <chrono>
#include <signal.h>

#ifdef POSIX

/**
 * @brief Возвращает время с момента запуска модуля
 * @return Возвращает время с момента запуска модуля
 */
 double ElapsedTime ()
{
    static std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds> (current_time - start_time).count();
    return elapsed;
    
}
#endif

/**
 * @brief Печать текстово строки заданного формата
 * @param  цвет выводимого текста
 * @param строка для вывода
 */
 
 
void console_print(const char *color, const char *format, ... )
{
#ifdef POSIX
    va_list args;
    va_start(args, format);
    printf("%s", color);
    vprintf(format, args);
    printf("%s", TURBO_COLOR_RESET);
    fflush(stdout);
    va_end(args);
#else
    static_assert ( false, "Console_print for realised only for POSIX!!" )		
                            //ConColorMsg(Color(255, 0, 0), "`%s'", lua_tostring(L, i));
#endif
}

/**
 * @brief Перехватчик сигнала segfault
 * @param  сигнал
 */

static void memento()
{
    printf("OOOOOps! I'm did it again!\n");
    fflush(stdout);
}

void posix_death_signal(int signum)
{
	memento(); // прощальные действия
        signal(signum, SIG_DFL); // перепосылка сигнала
	exit(3); //выход из программы. Если не сделать этого, то обработчик будет вызываться бесконечно.
}
