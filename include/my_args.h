#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <my_str.h>

#define arg_str(argc, argv, idx) ((idx) < (argc) && (idx) >= 0 ? (argv)[idx] : 0)
#define arg_exist(argc, argv, idx) ((idx) < (argc) && (idx) >= 0 ? 1 : 0)

    void my_args(int argc, char *argv[], int optc, const char *optv[], int *pidx);
    int arg_int32(int argc, char *argv[], int idx);

#ifdef __cplusplus
}
#endif
