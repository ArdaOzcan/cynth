#include "clog.h"

#include <stdio.h>
#include <time.h>

static ClogLogLevel log_level = CLOG_LOG_LEVEL_DEBUG;

static void
clog_print_styled(ClogStyle style, const char* msg, ...)
{
    printf("\x1b[%dm", style);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    printf("\x1b[%dm", CLOG_STYLE_RESET);
}

static void
clog_print_styled_no_reset(ClogStyle style, const char* msg, ...)
{
    printf("\x1b[%dm", style);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
}

static void
clog_print_colored(ClogColor color, const char* msg, ...)
{
    printf("\x1b[%dm", color);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    printf("\x1b[%dm", CLOG_STYLE_RESET);
}

static void
clog_print_colored_no_reset(ClogColor color, const char* msg, ...)
{
    printf("\x1b[%dm", color);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
}

static void
clog_print_styled_and_colored(ClogStyle style,
                              ClogColor color,
                              const char* msg,
                              ...)
{
    printf("\x1b[%d;%dm", style, color);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    printf("\x1b[%dm", CLOG_STYLE_RESET);
}

static void
clog_print_styled_and_colored_no_reset(ClogStyle style,
                                       ClogColor color,
                                       const char* msg,
                                       ...)
{
    printf("\x1b[%d;%dm", style, color);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    printf("\x1b[%dm", CLOG_STYLE_RESET);
}

void
clog_log_level_set(ClogLogLevel new_level)
{
    log_level = new_level;
}

ClogLogLevel
clog_log_level_get(void)
{
    return log_level;
}

void
clog_printf_debug(const char* msg, ...)
{
    if (log_level > CLOG_LOG_LEVEL_DEBUG)
        return;

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
}

void
clog_printf_info(const char* msg, ...)
{
    if (log_level > CLOG_LOG_LEVEL_INFO)
        return;

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
}

void
clog_printf_warning(const char* msg, ...)
{
    if (log_level > CLOG_LOG_LEVEL_WARNING)
        return;

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
}

void
clog_printf_error(const char* msg, ...)
{
    if (log_level > CLOG_LOG_LEVEL_ERROR)
        return;

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
}

void
clog_log_debug(const char* file_name, size_t line_number, const char* msg, ...)
{
    if (log_level > CLOG_LOG_LEVEL_DEBUG)
        return;

    clog_print_styled_and_colored_no_reset(
      CLOG_STYLE_BOLD, CLOG_COLOR_WHITE, "[");
    clog_print_styled_and_colored_no_reset(
      CLOG_STYLE_BOLD, CLOG_COLOR_BLUE, "%s:%u DEBUG", file_name, line_number);
    clog_print_styled_and_colored(CLOG_STYLE_BOLD, CLOG_COLOR_WHITE, "] ");

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    printf("\n");
}

void
clog_log_info(const char* file_name, size_t line_number, const char* msg, ...)
{
    if (log_level > CLOG_LOG_LEVEL_INFO)
        return;

    clog_print_styled_and_colored_no_reset(
      CLOG_STYLE_BOLD, CLOG_COLOR_WHITE, "[");
    clog_print_styled_and_colored_no_reset(
      CLOG_STYLE_BOLD, CLOG_COLOR_GREEN, "%s:%u INFO", file_name, line_number);
    clog_print_styled_and_colored(CLOG_STYLE_BOLD, CLOG_COLOR_WHITE, "] ");

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    printf("\n");
}

void
clog_log_warning(const char* file_name,
                 size_t line_number,
                 const char* msg,
                 ...)
{
    if (log_level > CLOG_LOG_LEVEL_WARNING)
        return;

    clog_print_styled_and_colored_no_reset(
      CLOG_STYLE_BOLD, CLOG_COLOR_WHITE, "[");
    clog_print_styled_and_colored_no_reset(CLOG_STYLE_BOLD,
                                           CLOG_COLOR_YELLOW,
                                           "%s:%u WARNING",
                                           file_name,
                                           line_number);
    clog_print_styled_and_colored(CLOG_STYLE_BOLD, CLOG_COLOR_WHITE, "] ");

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    printf("\n");
}

void
clog_log_error(const char* file_name, size_t line_number, const char* msg, ...)
{
    if (log_level > CLOG_LOG_LEVEL_ERROR)
        return;

    clog_print_styled_and_colored_no_reset(
      CLOG_STYLE_BOLD, CLOG_COLOR_WHITE, "[");
    clog_print_styled_and_colored_no_reset(
      CLOG_STYLE_BOLD, CLOG_COLOR_RED, "%s:%u ERROR", file_name, line_number);
    clog_print_styled_and_colored(CLOG_STYLE_BOLD, CLOG_COLOR_WHITE, "] ");

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    printf("\n");
}

void
clog_print_array_int(int* arr, size_t length)
{
    const size_t per_line = 16;
    size_t i = 0;

    printf("[ ");
    for (; i < length; i++) {
        printf("%d", arr[i]);
        if (i == length - 1)
            continue;

        if (i % per_line == per_line - 1) {
            printf("\n  ");
        } else {
            printf(", ");
        }
    }
    printf(" ]\n");
}

void
clog_print_array_u8(u8* arr, size_t length)
{
    const size_t per_line = 16;
    size_t i = 0;

    printf("[ ");
    for (; i < length; i++) {
        printf("%u", arr[i]);
        if (i == length - 1)
            continue;

        if (i % per_line == per_line - 1) {
            printf("\n  ");
        } else {
            printf(", ");
        }
    }
    printf(" ]\n");
}

void
clog_progress_init(ClogProgressBar* p, uint64_t total, int width)
{
    p->total = total;
    p->current = 0;
    p->bar_width = width > 0 ? width : 50;
    p->start_time = time(NULL);
}

void
clog_progress_render(ClogProgressBar* p)
{
    double frac = p->total ? (double)p->current / p->total : 0.0;
    if (frac > 1.0)
        frac = 1.0;

    int filled = (int)(frac * p->bar_width);

    printf("\r[");
    int i = 0;
    for (i = 0; i < p->bar_width; i++) {
        if (i < filled)
            printf("=");
        else if (i == filled)
            printf(">");
        else
            printf(" ");
    }

    double passed = difftime(time(NULL), p->start_time);
    double eta = (passed / frac) * (1.0 - frac);
    int mm = (int)eta / 60;
    printf("] %3.0f%% (%lu/%lu) %02d:%02d",
           frac * 100,
           p->current,
           p->total,
           mm,
           (int)(eta - 60.0 * mm));
    fflush(stdout);
}

void
clog_progress_increment(ClogProgressBar* p, uint64_t delta)
{
    p->current += delta;
    if (p->current > p->total)
        p->current = p->total;
    clog_progress_render(p);
}

void
clog_progress_finish(ClogProgressBar* p)
{
    p->current = p->total;
    clog_progress_render(p);
    printf("\n");
    fflush(stdout);
}
