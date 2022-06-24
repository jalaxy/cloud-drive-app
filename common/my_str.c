#include <my_str.h>

#define IS_BLANK(ch) ((ch) == ' ' || ((ch) == '\n') || (ch) == '\r' || (ch) == '\t')
#define IS_NUM(ch) ((ch) <= '9' && (ch) >= '0')

int to_int32(const char *str)
{
    unsigned long long ret = 0;
    int sign = 1;
    const char *p = str;
    while (*p && IS_BLANK(*p))
        p++;
    while (*p == '-' || *p == '+')
    {
        if (*p == '-')
            sign = -sign;
        p++;
    }
    while (*p && IS_NUM(*p))
    {
        ret *= 10;
        ret += *p - '0';
        p++;
        if (ret > 0x7fffffff)
            return INT32_FAILURE;
    }
    return *p ? INT32_FAILURE : sign * (int)ret;
}
