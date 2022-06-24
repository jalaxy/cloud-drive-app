#pragma once

#define TRY_NONZERO(st) \
    if ((st) == 0)      \
    perror(""), exit(1)
#define SAFE_RELEASE_ARR(p) \
    if (p)                  \
    delete[] p
#define BODY_TILLEND -1

class http_msg
{
public:
    http_msg();
    ~http_msg();
    http_msg(const http_msg &b);
    http_msg(int sz, const char *stream);
    http_msg(const char *filename, ...);
    http_msg &operator=(const http_msg &b);
    char *get_header(), *get_body();
    int get_header_sz(), get_body_sz();
    int get_msg(char *buf);
    char *query_header(const char *name);
    void construct_from_string(const char *stream, int sz);

private:
    char *header, *body;
    int header_sz, body_sz;
};

void url_escape(char *dst, const char *src);
