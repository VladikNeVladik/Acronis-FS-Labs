// No copyright. 2020, Vladislav Aleinik
#ifndef IO_RING_COPY_LOGGING_H
#define IO_RING_COPY_LOGGING_H

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

//================
// Logging levels 
//================

// Log level
#ifndef LOG_LEVEL
#define LOG_LEVEL 2U
#endif

// Log levels:
#define LOG_LVL_NO_LOG 0U
#define LOG_LVL_ERRORS 1U
#define LOG_LVL_LOGS   2U

//====================
// Timestamp printout 
//====================

const char* form_timestamp();

#define PRINT_FORMAT_WITH_TIMESTAMP(stream, log_type, format, ...)                                          \
do                                                                                                          \
{                                                                                                           \
    struct timeval __cur_time;                                                                              \
    const char* __time_str = form_timestamp(&__cur_time);                                                   \
                                                                                                            \
    fprintf(stream, "[%s %s:%06ld] "format"\n", log_type, __time_str, __cur_time.tv_usec, ##__VA_ARGS__);   \
} while (0)

//=========
// Logging
//=========

#define BUG_ON(condition, format, ...)                                      \
do                                                                          \
{                                                                           \
    if ((condition))                                                        \
    {                                                                       \
        PRINT_FORMAT_WITH_TIMESTAMP(stderr, "BUG", format, ##__VA_ARGS__);  \
        exit(EXIT_FAILURE);                                                 \
    }                                                                       \
} while (0)


#if LOG_LEVEL < LOG_LVL_ERRORS
    #define LOG_ERROR(...) do {} while (0)
#else
    #define LOG_ERROR(format, ...)                                              \
    do                                                                          \
    {                                                                           \
        PRINT_FORMAT_WITH_TIMESTAMP(stderr, "ERROR", format, ##__VA_ARGS__);    \
        exit(EXIT_FAILURE);                                                     \
    } while (0)
#endif


#if LOG_LEVEL < LOG_LVL_LOGS
    #define LOG(...) do {} while (0)
#else
    #define LOG(format, ...)                                                \
    do                                                                      \
    {                                                                       \
        PRINT_FORMAT_WITH_TIMESTAMP(stdout, "LOG", format, ##__VA_ARGS__);  \
    } while (0)
#endif

#endif // IO_RING_COPY_LOGGING_H
