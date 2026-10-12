#ifndef CLOG_H
#define CLOG_H

#include "ccore.h"
#include <stdarg.h>
#include <sys/types.h>

#define CLOG_DEBUG(msg, ...)                                                   \
    clog_log_debug(__FILE_NAME__, __LINE__, msg, ##__VA_ARGS__)

#define CLOG_INFO(msg, ...)                                                    \
    clog_log_info(__FILE_NAME__, __LINE__, msg, ##__VA_ARGS__)

#define CLOG_WARNING(msg, ...)                                                 \
    clog_log_warning(__FILE_NAME__, __LINE__, msg, ##__VA_ARGS__)

#define CLOG_ERROR(msg, ...)                                                   \
    clog_log_error(__FILE_NAME__, __LINE__, msg, ##__VA_ARGS__)

typedef enum
{
    CLOG_LOG_LEVEL_DEBUG = 0,
    CLOG_LOG_LEVEL_INFO = 1,
    CLOG_LOG_LEVEL_WARNING = 2,
    CLOG_LOG_LEVEL_ERROR = 3,
    CLOG_LOG_LEVEL_NONE = 4,
} ClogLogLevel;

typedef enum
{
    CLOG_COLOR_BLACK = 30,
    CLOG_COLOR_RED = 31,
    CLOG_COLOR_GREEN = 32,
    CLOG_COLOR_YELLOW = 33,
    CLOG_COLOR_BLUE = 34,
    CLOG_COLOR_MAGENTA = 35,
    CLOG_COLOR_CYAN = 36,
    CLOG_COLOR_WHITE = 37,
    CLOG_COLOR_DEFAULT = 39,
} ClogColor;

typedef enum
{
    CLOG_STYLE_RESET = 0,
    CLOG_STYLE_BOLD = 1,
    CLOG_STYLE_DIM = 2,
    CLOG_STYLE_ITALIC = 3,
    CLOG_STYLE_UNDERLINE = 4,
    CLOG_STYLE_BLINKING = 5,
    CLOG_STYLE_INVERSE = 7,
    CLOG_STYLE_HIDDEN = 8,
    CLOG_STYLE_STRIKETHROUGH = 9,
} ClogStyle;

typedef struct
{
    uint64_t total;
    uint64_t current;
    int bar_width;
    time_t start_time;
} ClogProgressBar;

void
clog_progress_init(ClogProgressBar* p, uint64_t total, int width);

void
clog_progress_render(ClogProgressBar* p);

void
clog_progress_increment(ClogProgressBar* p, uint64_t delta);

void
clog_progress_finish(ClogProgressBar* p);

void
clog_printf_debug(const char* msg, ...);

void
clog_printf_info(const char* msg, ...);

void
clog_printf_warning(const char* msg, ...);

void
clog_printf_error(const char* msg, ...);

void
clog_log_level_set(ClogLogLevel new_level);

ClogLogLevel
clog_log_level_get(void);

void
clog_log_debug(const char* file_name, size_t line_number, const char* msg, ...);

void
clog_log_info(const char* file_name, size_t line_number, const char* msg, ...);

void
clog_log_warning(const char* file_name,
                 size_t line_number,
                 const char* msg,
                 ...);

void
clog_log_error(const char* file_name, size_t line_number, const char* msg, ...);

void
clog_print_array_int(int* array, size_t length);

void
clog_print_array_u8(u8* arr, size_t length);

#endif /* CLOG_H */
