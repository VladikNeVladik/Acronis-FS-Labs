// No copyright. 2021, Vladislav Aleinik
#ifndef IO_RING_COPY_LOGGING_H
#define IO_RING_COPY_LOGGING_H

#include <stdarg.h>
#include <stdio.h>

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

//=======================
// Format with timestamp 
//=======================

void print_format_with_timestamp(FILE* stream, const char* log_type, const char* format, ...);

//=========
// Logging
//=========

#define BUG_ON(condition, format, ...)                                      \
do                                                                          \
{                                                                           \
    if ((condition))                                                        \
    {                                                                       \
        print_format_with_timestamp(stderr, "BUG", format, ##__VA_ARGS__);  \
        exit(EXIT_FAILURE);                                                 \
    }                                                                       \
} while (0)


#if LOG_LEVEL < LOG_LVL_ERRORS
    #define LOG_ERROR(...) do {} while (0)
#else
    #define LOG_ERROR(format, ...)                                              \
    do                                                                          \
    {                                                                           \
        print_format_with_timestamp(stderr, "ERROR", format, ##__VA_ARGS__);    \
    } while (0)
#endif


#if LOG_LEVEL < LOG_LVL_LOGS
    #define LOG(...) do {} while (0)
#else
    #define LOG(format, ...)                                                \
    do                                                                      \
    {                                                                       \
        print_format_with_timestamp(stdout, "LOG", format, ##__VA_ARGS__);  \
    } while (0)
#endif

#endif // IO_RING_COPY_LOGGING_H
