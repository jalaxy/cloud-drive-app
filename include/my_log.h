#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#define LOG(log, ...) freopen(log, "a+", stdout), \
                      printf(__VA_ARGS__),        \
                      fclose(log ? stdout : stdin);
#define LOGTIME(log, ...) freopen(log, "a+", stdout), \
                          printf("["),                \
                          print_time(),               \
                          printf("] "),               \
                          printf(__VA_ARGS__),        \
                          fclose(log ? stdout : stdin)
#define SECS_PER_DAY (24 * 3600)

    int print_time();
    int epoch_time();

#ifdef __cplusplus
}
#endif
