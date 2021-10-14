// No copyright. 2020, Vladislav Aleinik
#include <logging.h>

#include <unistd.h>
#include <stdlib.h>

#include <sys/time.h>
#include <time.h>

// Static buffer to print timestamp to:
#define TIMESTAMP_SIZE 64
#define LOG_SIZE 128

void print_format_with_timestamp(FILE* stream, const char* log_type, const char* format, ...)
{
    // Get function argument list:
    va_list arg_ptr;
    va_start(arg_ptr, format);

    // Form timestamp:
    struct timeval cur_time;
    char timestamp[TIMESTAMP_SIZE];

    if (gettimeofday(&cur_time, NULL) == -1)
    {
        fprintf(stderr, "[ERROR] Unable to get time of day\n");
        exit(EXIT_FAILURE);
    }

    struct tm* broken_down_time = localtime(&cur_time.tv_sec);
    if (broken_down_time == NULL)
    {
        fprintf(stderr, "[ERROR] Unable to get broken-down time\n");
        exit(EXIT_FAILURE);
    }

    if (strftime(timestamp, TIMESTAMP_SIZE, "%Y-%m-%d %H:%M:%S", broken_down_time) == 0)
    {
       fprintf(stderr, "[ERROR] Unable to get a nice readable time string\n");
       exit(EXIT_FAILURE);
    }

    // Perform logging:
    char log_buffer[LOG_SIZE];
    int log_size = 0;

    log_size += snprintf (&log_buffer[log_size], LOG_SIZE - log_size, "[%s %s:%06ld] ", log_type, timestamp, cur_time.tv_usec);
    log_size += vsnprintf(&log_buffer[log_size], LOG_SIZE - log_size, format, arg_ptr);
    log_size += snprintf (&log_buffer[log_size], LOG_SIZE - log_size, "\n");

    log_buffer[log_size] = '\0';

    fprintf(stream, "%s", log_buffer);
}