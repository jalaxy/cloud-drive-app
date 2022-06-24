#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

    int alloc_and_parse_config(
        const char *fname, const char *locale, char **pnames[], char **pvalues[]);
    void free_config(int n, char *names[], char *values[]);
    int query_config(char *str, int n, char *names[]);

#ifdef __cplusplus
}
#endif
