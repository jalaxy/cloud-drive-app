#include <my_daemon.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
/**
 * @brief Fork a process to daemon
 *
 */
void my_daemon(const char *wd)
{
    umask(0);
    chdir(wd);
    if (fork())
        exit(EXIT_SUCCESS);
    setsid();
    for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++)
        close(i);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    if (fork())
        exit(EXIT_SUCCESS);
}
