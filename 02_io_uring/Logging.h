#ifndef IO_RING_COPY_LOGGING_H
#define IO_RING_COPY_LOGGING_H

// Time:
#include <sys/time.h>
#include <time.h>

//==================
// Logging facility 
//==================

// Log level
#ifndef LOG_LEVEL
#define LOG_LEVEL 2U
#endif

// Log levels:
#define LOG_LVL_NO_LOG 0U
#define LOG_LVL_BUGS   1U
#define LOG_LVL_ERRORS 2U
#define LOG_LVL_LOGS   3U

#define PRINT_FORMAT_WITH_TIMESTAMP(stream, log_type, format, ...)                                              \
do                                                                                                              \
{                                                                                                               \
    struct timeval __cur_time;                                                                                  \
                                                                                                                \
    if (gettimeofday(&__cur_time, NULL) == -1)                                                                  \
    {                                                                                                           \
        fprintf(stderr, "[ERROR] Unable to get time of day\n");                                                 \
        exit(EXIT_FAILURE);                                                                                     \
    }                                                                                                           \
                                                                                                                \
    struct tm* __broken_down_time = localtime(&__cur_time.tv_sec);                                              \
    if (__broken_down_time == NULL)                                                                             \
    {                                                                                                           \
        fprintf(stderr, "[ERROR] Unable to get broken-down time\n");                                            \
        exit(EXIT_FAILURE);                                                                                     \
    }                                                                                                           \
                                                                                                                \
    char __time_str_buf[128];                                                                                   \
    if (strftime(__time_str_buf, sizeof(__time_str_buf), "%Y-%m-%d %H:%M:%S", __broken_down_time) == 0)         \
    {                                                                                                           \
       fprintf(stderr, "[ERROR] Unable to get a nice readable time string\n");                                  \
       exit(EXIT_FAILURE);                                                                                      \
    }                                                                                                           \
                                                                                                                \
    fprintf(stream, "[%s %s:%06ld] "format"\n", log_type, __time_str_buf, __cur_time.tv_usec, ##__VA_ARGS__);   \
} while (0)

#if LOG_LEVEL < LOG_LVL_BUGS
    #define BUG_ON(...) do {} while (0)
#else
#define BUG_ON(condition, format, ...)                                          \
    do                                                                          \
    {                                                                           \
        if ((condition))                                                        \
        {                                                                       \
            PRINT_FORMAT_WITH_TIMESTAMP(stderr, "BUG", format, ##__VA_ARGS__);  \
            exit(EXIT_FAILURE);                                                 \
        }                                                                       \
    } while (0)
#endif

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
