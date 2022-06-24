#include <my_args.h>

/**
 * @brief parse arguments of entry point
 *
 * @param argc argument count
 * @param argv argument vector
 * @param optc option count, size of optv
 * @param optv user-defined string vector of vector name
 * @param idx first output option value index in argv. argc if not found
 */
void my_args(int argc, char *argv[], int optc, const char *optv[], int *pidx)
{
    for (int i = 0; i < optc; i++)
        pidx[i] = -1;
    for (int i = 0; i < optc; i++)
    {
        int j;
        for (j = 1; j < argc; j++)
        {
            int k = 0, eq = 1;
            while (optv[i][k] && argv[j][k] && optv[i][k] == argv[j][k])
                k++;
            if (optv[i][k] == 0 && argv[j][k] == 0)
                break;
        }
        pidx[i] = j;
    }
}

int arg_int32(int argc, char *argv[], int idx)
{
    if (idx < argc && idx >= 0)
        return to_int32(argv[idx]);
    return INT32_FAILURE;
}
