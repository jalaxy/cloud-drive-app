#include <my_log.h>
#include <locale.h>
#include <sys/time.h>

#define LEAP(year) (!((year) % 400) || !((year) % 4) && (year) % 100)
#define DAYS_Y(year) (((year) - (1970)) * 365 + (year - 1) / 4 - (year - 1) / 100 + (year - 1) / 400 - 477)
#define DAYS_M(month, year) ((LEAP(year) ? days_l : days_n)[month])

const int days_l[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366};
const int days_n[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
FILE *_log_pointer;

int print_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int year, month, day, hour, minute, second, millisecond, week;
    millisecond = tv.tv_usec / 1000;
    second = tv.tv_sec;
    minute = second / 60;
    second %= 60;
    hour = minute / 60 + 8;
    minute %= 60;
    day = hour / 24;
    hour %= 24;
    year = 1970 + day / 365.2425 - 1; // approximate year [-1, 1]
    if (DAYS_Y(year + 1) <= day)
        year++;
    if (DAYS_Y(year + 1) <= day)
        year++;
    day -= DAYS_Y(year);
    month = day / 30. - 1;
    if (DAYS_M(month + 1, year) <= day)
        month++;
    if (DAYS_M(month + 1, year) <= day)
        month++;
    day -= DAYS_M(month, year);

    printf("%04d-%02d-%02d %02d:%02d:%02d.%03d",
           year, month + 1, day + 1, hour, minute, second, millisecond);
}

int epoch_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}
