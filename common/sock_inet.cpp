#include <sock_inet.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <ifaddrs.h>
#include <my_args.h>

// argument options name
const char *arg_options[] = {"--port", "--ip", "--myport"};

// whether to exit when TRY() macro failed
bool ignore_error = false;

/**
 * @brief Construct a new socket inet base object
 *
 * @param recv_buf_sz receiving buffer size
 * @param send_buf_sz sending buffer size
 * @param blocking whether to block
 */
sock_inet_base::sock_inet_base(int recv_buf_sz, int send_buf_sz, bool blocking)
{
    TRY(fd = socket(AF_INET, SOCK_STREAM, 0), "socket()");
    count_send = count_recv = 0;
    if (recv_buf_sz)
        TRY(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_sz, 4), "setsockopt()");
    if (send_buf_sz)
        TRY(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buf_sz, 4), "setsockopt()");
    int optval = 1;
    TRY(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)), "setsockopt()");
    TRY(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(int)), "setsockopt()");
    set_blocking(blocking);
}

/**
 * @brief Construct a new sock inet base object
 *
 * @param b another object
 */
sock_inet_base::sock_inet_base(const sock_inet_base &b)
{
    *this = b;
    ((sock_inet_base &)b).fd = -1;
}

/**
 * @brief Destroy the socket inet base object
 *
 */
sock_inet_base::~sock_inet_base()
{
    if (fd != -1)
        close(fd);
}

/**
 * @brief Get file descriptor of socket
 *
 * @return socket file descriptor
 */
int sock_inet_base::get_fd() const { return fd; }

/**
 * @brief Get the receiving buffer size
 *
 * @return size of receiving buffer size
 */
int sock_inet_base::get_recv_buffer_sz() const
{
    socklen_t len = 4;
    int ret, suc;
    TRY(suc = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &ret, &len), "getsockopt()");
    return suc ? -1 : ret;
}

/**
 * @brief Get the sending buffer size
 *
 * @return size of sending buffer size
 */
int sock_inet_base::get_send_buffer_sz() const
{
    socklen_t len = 4;
    int ret, suc;
    TRY(suc = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &ret, &len), "getsockopt()");
    return suc ? -1 : ret;
}

/**
 * @brief Set the receiving buffer size
 *
 * @param sz size to set
 * @return whether successful
 */
int sock_inet_base::set_recv_buffer_sz(int sz)
{
    int ret;
    TRY(ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sz, 4), "setsockopt()");
    return ret;
}

/**
 * @brief Set the sending buffer size
 *
 * @param sz size to set
 * @return whether successful
 */
int sock_inet_base::set_send_buffer_sz(int sz)
{
    int ret;
    TRY(ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sz, 4), "setsockopt()");
    return ret;
}

/**
 * @brief Receiving data from socket
 *
 * @param buffer data buffer
 * @param n size of data to read in bytes
 * @return number of bytes received successfully
 */
int sock_inet_base::recv_data(void *buffer, int n)
{
    int ret;
    TRY(ret = recv(fd, buffer, n, 0), "recv()");
    count_recv += ret;
    return ret;
}

/**
 * @brief Sending data to socket
 *
 * @param buffer data buffer
 * @param n size of data to write in bytes
 * @return number of bytes sent successfully
 */
int sock_inet_base::send_data(const void *buffer, int n)
{
    int ret;
    TRY(ret = send(fd, buffer, n, 0), "send()");
    count_send += ret;
    return ret;
}

/**
 * @brief Get Receiving data counting in bytes
 *
 * @return count of bytes received
 */
int sock_inet_base::get_count_recv() const { return count_recv; }

/**
 * @brief Get Sending data counting in bytes
 *
 * @return count of bytes sent
 */
int sock_inet_base::get_count_send() const { return count_send; }

/**
 * @brief Get local IP address
 *
 * @return the IPv4 address
 */
const char *sock_inet_base::get_local_ip() const { return inet_ntoa(local_addr.sin_addr); }

/**
 * @brief Get local IP port
 *
 * @return the port
 */
int sock_inet_base::get_local_port() const { return ntohs(local_addr.sin_port); }

/**
 * @brief Get local IP address
 *
 * @return the IPv4 address
 */
const char *sock_inet_base::get_remote_ip() const { return inet_ntoa(remote_addr.sin_addr); }

/**
 * @brief Get local IP port
 *
 * @return the port
 */
int sock_inet_base::get_remote_port() const { return ntohs(remote_addr.sin_port); }

/**
 * @brief Get local IP and port
 *
 * @return the address
 */
sockaddr_in sock_inet_base::get_local_addr() const { return local_addr; }

/**
 * @brief Get remote IP and port
 *
 * @return the address
 */
sockaddr_in sock_inet_base::get_remote_addr() const { return remote_addr; }

/**
 * @brief Set the blocking flag
 *
 * @param blocking whether to block
 * @return error code
 */
int sock_inet_base::set_blocking(bool blocking)
{
    int flags;
    TRY(flags = fcntl(fd, F_GETFL, 0), "fcntl()");
    TRY(fcntl(fd, F_SETFL, blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK), "fcntl()");
    return 0;
}

/**
 * @brief Set this socket about event poll
 *
 * @param pepfd the epoll descriptor
 * @param type 0: remove  1: add
 * @param inout 1: in  2: out  3: inout
 */
void sock_inet_base::set_epoll(int epfd, int ctl, unsigned int events)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    TRY(epoll_ctl(epfd, ctl, fd, &ev), "epoll_ctl()");
}

/**
 * @brief Construct a new sock inet listen object
 *
 * @param ip IPv4 address in dot format
 * @param rc socket sending buffer size
 * @param sd socket receiving buffer size
 * @param blocking whether to block
 */
sock_inet_listen::sock_inet_listen(
    const char *ip, int port,
    int rc, int sd, bool blocking) : sock_inet_base(rc, sd, blocking)
{
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = ip == NULL ? 0 : inet_addr(ip);
    local_addr.sin_port = htons(port);
    TRY(bind(fd, (struct sockaddr *)&local_addr, sizeof(local_addr)), "bind()");
    TRY(listen(fd, 32), "listen()");
}

/**
 * @brief Destroy the sock inet listen object
 *
 */
sock_inet_listen::~sock_inet_listen() {}

/**
 * @brief Construct a new sock inet accept object
 *
 * @param fd_listen
 */
sock_inet_accept::sock_inet_accept(
    const sock_inet_listen &sock_lsn, int rc, int sd, bool blocking)
{
    local_addr = sock_lsn.get_local_addr();
    socklen_t len = 16;
    TRY(fd = accept(sock_lsn.get_fd(), (struct sockaddr *)&remote_addr, &len), "accept()");
    set_blocking(blocking); // default set to blocking
    if (rc)
        set_recv_buffer_sz(rc);
    if (sd)
        set_send_buffer_sz(sd);
}

/**
 * @brief Destroy the sock inet accept object
 *
 */
sock_inet_accept::~sock_inet_accept() {}

/**
 * @brief Construct a new sock inet connect object
 *
 * @param ip remote IP address
 * @param myip local IP address
 * @param port remote port
 * @param myport local port
 * @param rc socket sending buffer size
 * @param sd socket receiving buffer size
 * @param blocking whether to block
 */
sock_inet_connect::sock_inet_connect(const char *myip, int myport, int rc, int sd, bool blocking)
    : sock_inet_base(rc, sd, blocking)
{
    // bind local ip and port
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = myip == NULL ? 0 : inet_addr(myip);
    local_addr.sin_port = htons(myport);
    TRY(bind(fd, (struct sockaddr *)&local_addr, sizeof(local_addr)), "bind()");
}

/**
 * @brief Connect to remote target
 *
 * @return connect() return value
 */
int sock_inet_connect::connect_remote(const char *ip, int port)
{
    // connect remote ip and port
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = ip == NULL ? 0 : inet_addr(ip);
    remote_addr.sin_port = htons(port);
    return connect(fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
}

/**
 * @brief Destroy the sock inet connect object
 *
 */
sock_inet_connect::~sock_inet_connect() {}

/**
 * @brief Parsing arguments for inet socket options
 *
 * @param argc count of arguments
 * @param argv vector of arguments
 * @param arg_flags which arguments to search
 * @param ... addresses of result values to write
 */
void inet_args(int argc, char *argv[], int arg_flags, ...)
{
    int *idx = new int[sizeof(arg_options) / sizeof(char *)];
    my_args(argc, argv, sizeof(arg_options) / sizeof(char *), arg_options, idx);
    int num = 0, x = arg_flags;
    do
        num += x & 1;
    while (x >>= 1);
    va_list ap;
    va_start(ap, arg_flags);
    if (arg_flags & ARGS_PORT)
        *va_arg(ap, int *) = arg_int32(argc, argv, idx[0] + 1);
    if (arg_flags & ARGS_IP)
        *va_arg(ap, char **) = arg_str(argc, argv, idx[1] + 1);
    if (arg_flags & ARGS_LPORT)
        *va_arg(ap, int *) = arg_int32(argc, argv, idx[2] + 1);
    va_end(ap);
    delete[] idx;
}

/**
 * @brief Print all host IP addresses
 *
 */
void print_ip()
{
    struct ifaddrs *addrs;
    getifaddrs(&addrs);
    printf("Address list:\n");
    for (struct ifaddrs *p = addrs; p; p = p->ifa_next)
        if (p->ifa_addr->sa_family == AF_INET)
            printf("    %s: %s\n", p->ifa_name,
                   inet_ntoa(((struct sockaddr_in *)p->ifa_addr)->sin_addr));
    freeifaddrs(addrs);
}
