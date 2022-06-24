#include <my_log.h>
#include <unistd.h>
#include <errno.h>
#include <my_args.h>
#include <my_daemon.h>
#include <sock_inet.h>
#include <signal.h>
#include <errno.h>
#include <new>
#include <mysql.h>

#define SOCK_BUF_SZ (1024 * 16)
#define FILE_BUF_SZ (1024 * 1024)

typedef struct struct_sock_list
{
    sock_inet_base *sock;
    struct struct_sock_list *next;
} sock_list_t;

typedef struct struct_write_queue
{
    int offset, size;
    char buf[FILE_BUF_SZ];
    sock_inet_base *sock; // destination socket
    struct struct_write_queue *next;
} write_queue_t;

bool exit_signal;

void sighandler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        exit_signal = true;
}

void logonclose(const char *logname, sock_inet_base *p)
{
    LOGTIME(logname, "Connection with %s:%d closed with IN/OUT: %d/%d.\n",
            p->get_remote_ip(), p->get_remote_port(),
            p->get_count_recv(), p->get_count_send());
}

void processincoming(const char *logname, MYSQL *mysql, const char *buf)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    if (mysql_query(mysql, "select * from user"))
    {
        LOGTIME(logname, "mysql_query failed(%s)\n", mysql_error(mysql));
        return;
    }
    if (!(result = mysql_store_result(mysql)))
    {
        LOGTIME(logname, "mysql_store_result failed\n");
        return;
    }
    LOGTIME(logname, "select return %d records\n", (int)mysql_num_rows(result));
    while ((row = mysql_fetch_row(result)) != NULL)
        LOGTIME(logname, "%s\n%s\n%s\n", row[0], row[1], row[2]);
    mysql_free_result(result);
}

int main(int argc, char *argv[])
{
    // process arguments
    const char *options[] = {"--daemon", "--port", "--logname", "--help"};
    int idx[4];
    my_args(argc, argv, 4, options, idx);
    bool daemon = arg_exist(argc, argv, idx[0]);
    int port = arg_int32(argc, argv, idx[1] + 1);
    char *logname = arg_str(argc, argv, idx[2] + 1);
    bool info = arg_exist(argc, argv, idx[3]);
    if (port & 0xffff0000)
        fprintf(stderr, "Invalid argument.\n"), info = true;
    if (info)
    {
        printf("A simple cloud drive server. Arguments:\n");
        printf("    --daemon       Run as daemon.\n");
        printf("    --port PORT    Listen on port PORT.\n");
        printf("    --logname s    Store log in the path s.\n");
        printf("    --help         Show information.\n");
        return 0;
    }
    if (daemon)
        my_daemon("/");

    // prepare system and database
    LOGTIME(logname, "Proxy starts running.\n");
    MYSQL *mysql;
    if (!(mysql = mysql_init(NULL)))
        LOGTIME(logname, "mysql_init failed\n"), exit(0);
    if (!mysql_real_connect(
            mysql, "localhost", "root", "root123", "cloud_drive", 0, 0, 0))
        LOGTIME(logname, "mysql_real_connect failed(%s)\n", mysql_error(mysql));
    mysql_set_character_set(mysql, "utf8");

    // non-blocking listen and poll initialization
    sock_inet_listen socklsn(0, port, SOCK_BUF_SZ, SOCK_BUF_SZ, false);
    int epfd = epoll_create1(0);
    TRY(epfd, "epoll_create1()");
    struct epoll_event events[256];
    socklsn.set_epoll(epfd, 1);

    // socket list and buffer queue initialization
    sock_list_t head = {NULL, NULL};
    write_queue_t front = {0, 0, {0}, NULL, NULL}, *rear = &front;

    // signals
    exit_signal = false; // exit_signal is set after receiving SIGINT or SIGTERM
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    // endless loop
    while (!exit_signal)
    {
        int nfds = epoll_wait(epfd, events, 256, -1); // wait for polling
        if (nfds == -1)
            if (errno == EINTR) // interrupted by signal, which is handled
                continue;
            else
                perror("epoll_wait()"), exit(1); // other errno is abnormal
        for (int i = 0; i < nfds; i++)
            if (events[i].data.fd == socklsn.get_fd())
                // listening socket get input
                // loop to establish new (maybe multiple) connections
                while (true)
                {
                    sock_list_t *p = new (std::nothrow) sock_list_t;
                    IGNORE_ERR(
                        p->sock = new (std::nothrow) sock_inet_accept(
                            socklsn, SOCK_BUF_SZ, SOCK_BUF_SZ, false));
                    if (p->sock->get_fd() == -1) // accept no more connections
                        break;
                    // otherwise make a new connection
                    // inet_ntoa calculate in a specific, same position of memory
                    LOGTIME(logname, "Connected with %s:%d\n",
                            p->sock->get_remote_ip(), p->sock->get_remote_port());
                    p->next = head.next;
                    head.next = p; // link into list
                    p->sock->set_epoll(epfd, EPOLL_CTL_ADD);
                }
            else // normal data I/O socket
            {
                if (events[i].events & EPOLLIN) // get something to read
                {
                    sock_list_t *p = &head, *pre;
                    while (p->next && events[i].data.fd != p->next->sock->get_fd())
                        p = p->next;
                    if (!(p = (pre = p)->next)) // no record, which is not intended
                        continue;
                    // processing data
                    char buf[SOCK_BUF_SZ];
                    int size;
                    IGNORE_ERR(size = p->sock->recv_data(buf, SOCK_BUF_SZ));
                    processincoming(logname, mysql, buf);
                    if (size <= 0)
                    {
                        // connection is closed by any side of connection
                        pre->next = p->next;
                        p->sock->set_epoll(epfd, EPOLL_CTL_DEL);
                        logonclose(logname, p->sock);
                        delete p->sock;
                        delete p;
                    }
                }
                if (events[i].events & EPOLLOUT)
                    for (write_queue_t *pre = &front; pre && pre->next; pre = pre->next)
                        if (events[i].data.fd == pre->next->sock->get_fd())
                        {
                            if (rear == pre->next)
                                rear = pre;
                            write_queue_t *p = pre->next;
                            pre->next = p->next;
                            p->sock->send_data(p->buf, p->size);
                            delete p;
                            break; // once at a time
                        }
            }
        // only set sockets in buffer list with EPOLLOUT
        for (sock_list_t *p = head.next; p; p = p->next)
            p->sock->set_epoll(epfd, EPOLL_CTL_MOD);
        for (write_queue_t *p = front.next; p; p = p->next)
            p->sock->set_epoll(epfd, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
    }

    // cleanup after termination
    for (sock_list_t *p = head.next; p != NULL; p = p->next)
        logonclose(logname, p->sock);
    close(epfd);
    sock_list_t *p_s = head.next;
    while (p_s)
    {
        sock_list_t *q = p_s;
        p_s = p_s->next;
        delete q->sock;
        delete q;
    }
    write_queue_t *p_q = front.next;
    while (p_q)
    {
        write_queue_t *q = p_q;
        p_q = p_q->next;
        delete q;
    }
    LOGTIME(logname, "Proxy process exits.\n");
    mysql_close(mysql);

    return 0;
}
