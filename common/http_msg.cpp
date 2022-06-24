#include <http_msg.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdarg.h>

#define IS_BLANK(ch) ((ch) == ' ' || (ch) == '\t' || (ch) == '\n')
#define IS_HEX(ch) ((ch) >= '0' && (ch) <= '9' || (ch) >= 'A' && (ch) <= 'F' || (ch) >= 'a' && (ch) <= 'f')
#define IS_NUM(ch) ((ch) >= '0' && (ch) <= '9')
#define IS_LET(ch) ((ch) >= 'A' && (ch) <= 'Z' || (ch) >= 'a' && (ch) <= 'z')
#define SEP_LEN 2 // strlen("\r\n")

http_msg::http_msg()
{
    header = body = 0;
    header_sz = body_sz = 0;
}

http_msg::http_msg(const http_msg &b)
{
    this->header = new (std::nothrow) char[b.header_sz + 1];
    this->body = new (std::nothrow) char[b.body_sz + 1];
    TRY_NONZERO(this->header && this->body);
    memcpy(this->header, b.header, b.header_sz);
    memcpy(this->body, b.body, b.body_sz);
    this->header[header_sz = b.header_sz] = this->body[body_sz = b.body_sz] = 0;
}

http_msg::http_msg(int sz, const char *stream) { construct_from_string(stream, sz); }

http_msg::http_msg(const char *filename, ...)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
        printf("%s does not exist.\n", filename), exit(0);
    fseek(fp, 0, SEEK_END);
    int sz = ftell(fp);
    rewind(fp);
    char *format = new char[sz];
    fread(format, sizeof(char), sz, fp);
    va_list args;
    va_start(args, filename);
    sz = vsnprintf(NULL, 0, format, args);
    va_end(args);
    char *str = new char[sz + 100];
    va_start(args, filename);
    vsnprintf(str, sz + 100, format, args);
    va_end(args);
    delete[] format;
    construct_from_string(str, sz);
    delete[] str;
    fclose(fp);
}

http_msg::~http_msg()
{
    SAFE_RELEASE_ARR(this->header);
    SAFE_RELEASE_ARR(this->body);
}

http_msg &http_msg::operator=(const http_msg &b)
{
    SAFE_RELEASE_ARR(this->header);
    SAFE_RELEASE_ARR(this->body);
    this->header = new (std::nothrow) char[b.header_sz + 1];
    this->body = new (std::nothrow) char[b.body_sz + 1];
    TRY_NONZERO(this->header && this->body);
    memcpy(this->header, b.header, b.header_sz);
    memcpy(this->body, b.body, b.body_sz);
    this->header[header_sz = b.header_sz] = this->body[body_sz = b.body_sz] = 0;
    return *this;
}

/**
 * @brief Part of constructor fucntion with http message string
 *
 * @param stream the string, '\0' is required to search in headers
 * @param sz size of the string
 */
void http_msg::construct_from_string(const char *stream, int sz)
{
    header_sz = 0;
    header = NULL;
    body_sz = 0;
    body = NULL;
    const char *p;
    if ((p = strstr((char *)stream, "\r\n\r\n")) == NULL)
        return;
    header_sz = p - stream + SEP_LEN;
    TRY_NONZERO(header = new (std::nothrow) char[header_sz + 1]);
    memcpy(header, stream, header_sz);
    if (p = query_header("Content-Length"))
    {
        body_sz = strtol(p, NULL, 10);
        if (body_sz == 0 || header_sz + body_sz + SEP_LEN > sz)
            return;
        if (body_sz == BODY_TILLEND)
            body_sz = sz - header_sz - SEP_LEN;
        body = new (std::nothrow) char[body_sz + 1];
        memcpy(body, stream + header_sz + SEP_LEN, body_sz);
    }
    else if (p = query_header("Transfer-Encoding"))
    {
        if (strncmp(p, "chunked", strlen("chunked")) == 0)
        {
            const char *q = stream + header_sz + SEP_LEN;
            while (true)
            {
                char *ptr;
                int chksz = strtol(q, &ptr, 16);
                if ((!IS_HEX(*q) || strncmp(ptr, "\r\n", SEP_LEN)) && !(body_sz = 0) || !chksz)
                    break;
                q = ptr + SEP_LEN + chksz + SEP_LEN;
                body_sz += chksz;
            }
            if (!body_sz && !(body = NULL))
                return;
            TRY_NONZERO(body = new (std::nothrow) char[body_sz + 1]);
            body_sz = 0;
            q = stream + header_sz + SEP_LEN;
            while (true)
            {
                char *ptr;
                int chksz = strtol(q, &ptr, 16);
                if ((!IS_HEX(*q) || strncmp(ptr, "\r\n", SEP_LEN)) && !(body_sz = 0) || !chksz)
                    break;
                q = ptr + SEP_LEN;
                memcpy(body + body_sz, q, chksz);
                q += chksz + SEP_LEN;
                body_sz += chksz;
            }
        }
    }
    if (this->header)
        this->header[header_sz] = 0;
    if (this->body)
        this->body[body_sz] = 0;
}

char *http_msg::get_body() { return this->body; }

char *http_msg::get_header() { return this->header; }

int http_msg::get_body_sz() { return this->body_sz; }

int http_msg::get_header_sz() { return this->header_sz; }

int http_msg::get_msg(char *buf)
{
    if (buf)
    {
        memcpy(buf, header, header_sz);
        memcpy(buf + header_sz, "\r\n", SEP_LEN);
        memcpy(buf + header_sz + SEP_LEN, body, body_sz);
    }
    return header_sz + body_sz + SEP_LEN;
}

char *http_msg::query_header(const char *name)
{
    char *p = strstr(header, name);
    if (!p)
        return NULL;
    while (*p != ':')
        p++;
    p++;
    while (IS_BLANK(*p))
        p++;
    return p;
}

void url_escape(char *dst, const char *src)
{
    int i_esc = 0;
    for (int i = 0; i < strlen(src); i++)
        if (IS_NUM(src[i]) || IS_LET(src[i]) || src[i] == '_' ||
            src[i] == ',' || src[i] == '-' || src[i] == '.')
            dst[i_esc++] = src[i];
        else
        {
            dst[i_esc++] = '%';
            sprintf(dst + i_esc, "%02x", (unsigned char)src[i]);
            while (dst[i_esc])
                i_esc++;
        }
    dst[i_esc] = 0;
}
