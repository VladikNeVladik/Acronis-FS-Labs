// No copyright. 2020, Vladislav Aleinik
#include <logging.h>

#include <unistd.h>
#include <stdlib.h>

// Static buffer to print timestamp to:
#define TIMESTAMP_SIZE 64
static char TIMESTAMP_BUFFER[TIMESTAMP_SIZE];

const char* form_timestamp(struct timeval* cur_time)
{
    if (gettimeofday(cur_time, NULL) == -1)
    {
        fprintf(stderr, "[ERROR] Unable to get time of day\n");
        exit(EXIT_FAILURE);
    }

    struct tm* broken_down_time = localtime(&cur_time->tv_sec);
    if (broken_down_time == NULL)
    {
        fprintf(stderr, "[ERROR] Unable to get broken-down time\n");
        exit(EXIT_FAILURE);
    }

    if (strftime(TIMESTAMP_BUFFER, sizeof(TIMESTAMP_BUFFER), "%Y-%m-%d %H:%M:%S", broken_down_time) == 0)
    {
       fprintf(stderr, "[ERROR] Unable to get a nice readable time string\n");
       exit(EXIT_FAILURE);
    }

    return TIMESTAMP_BUFFER;
}