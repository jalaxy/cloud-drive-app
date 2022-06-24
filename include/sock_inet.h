#pragma once

#include <netinet/in.h>
#include <sys/epoll.h>
#include <cstdlib>

#define TRY(statement, errinfo)           \
    if ((statement) < 0 && !ignore_error) \
    exit((perror(errinfo), 1))
#define IGNORE_ERR(statement) ignore_error = true, statement, ignore_error = false
#define ARGS_PORT 0x00000001u // server port
#define ARGS_IP 0x00000002u
#define ARGS_LPORT 0x00000004u // local port

extern const char *arg_options[];
extern bool ignore_error;

// IPv4 socket base class
class sock_inet_base
{
protected:
    int fd, count_send, count_recv;
    sockaddr_in local_addr, remote_addr;

public:
    sock_inet_base(int recv_buf_sz = 0, int send_buf_sz = 0, bool blocking = true);
    sock_inet_base(const sock_inet_base &b);
    ~sock_inet_base();
    int get_fd() const;
    int get_recv_buffer_sz() const, set_recv_buffer_sz(int sz);
    int get_send_buffer_sz() const, set_send_buffer_sz(int sz);
    int recv_data(void *buffer, int n);
    int send_data(const void *buffer, int n);
    int get_count_recv() const, get_count_send() const;
    const char *get_local_ip() const, *get_remote_ip() const;
    int get_local_port() const, get_remote_port() const;
    sockaddr_in get_local_addr() const, get_remote_addr() const;
    int set_blocking(bool blocking);
    void set_epoll(int epfd, int ctl, unsigned int events = EPOLLIN);
    void rm_from_epoll(int epfd);
};

// socket for listening
class sock_inet_listen : public sock_inet_base
{
public:
    sock_inet_listen(
        const char *ip = 0, int port = 0,
        int rc = 0, int sd = 0, bool blocking = true);
    ~sock_inet_listen();
};

// socket for accepting
class sock_inet_accept : public sock_inet_base
{
public:
    sock_inet_accept(
        const sock_inet_listen &sock_lsn, int rc = 0, int sd = 0, bool blocking = true);
    ~sock_inet_accept();
};

// socket for connecting
class sock_inet_connect : public sock_inet_base
{
public:
    sock_inet_connect(
        const char *myip = 0, int myport = 0, int rc = 0, int sd = 0, bool blocking = true);
    ~sock_inet_connect();
    int connect_remote(const char *ip, int port);
};

void inet_args(int argc, char *argv[], int arg_flags, ...);

void print_ip();
