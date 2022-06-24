#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#define INT32_FAILURE (unsigned int)0x80000000

    int to_int32(const char *str);

#ifdef __cplusplus
}
#endif
