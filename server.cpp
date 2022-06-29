#include <my_log.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <my_args.h>
#include <my_daemon.h>
#include <sock_inet.h>
#include <signal.h>
#include <errno.h>
#include <new>
#include <mysql.h>
#include <sys/stat.h>
#include <dirent.h>

#define SOCK_BUF_SZ (1024 * 16)
#define MSG_BUF_SZ (1024 * 1024)

#define MSG_NOUSER "User does not exist."
#define MSG_WPSWD "Incorrect password."
#define MSG_WINV "Invalid invitation code."
#define MSG_UEXST "User already exists."
#define MSG_NFILE "File not found."
#define MSG_NSESS "Session not found."
#define MSG_UNREC "Unrecognizable data format."
#define MSG_TOOLRG "Request exceeds size limits."
#define MSG_NUPLD "Unrequested upload data."
#define MSG_ECREAT "Unable to create file."
#define MSG_WSHA "Wrong hash value."

#define STR_EQ(s, sref) (strncmp(s, sref, strlen(sref)) == 0)

typedef struct struct_sock_list
{
    sock_inet_base *sock;
    struct struct_sock_list *next;
} sock_list_t;

typedef struct struct_write_queue
{
    int size;
    char buf[MSG_BUF_SZ];
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

MYSQL_RES *create_query_result(const char *logname, MYSQL *mysql, const char *query, bool create)
{
    MYSQL_RES *result = NULL;
    if (mysql_query(mysql, query))
    {
        LOGTIME(logname, "mysql_query failed(%s)\n", mysql_error(mysql));
        return NULL;
    }
    if (create && !(result = mysql_store_result(mysql)))
        LOGTIME(logname, "mysql_store_result failed\n");
    return result;
}

const char *newsid(const char *logname, MYSQL *mysql, char *sid)
{
    char q[256] = {0};
    while (true)
    {
        for (int i = 0; i < 32; i++)
        {
            sid[i] = rand() % 16;
            sid[i] += sid[i] < 10 ? '0' : 'a' - 10;
        }
        sid[32] = 0;
        sprintf(q, "select count(*) from `sessions` where `sessionid` = '%s'", sid);
        MYSQL_RES *res = create_query_result(logname, mysql, q, true);
        bool exists = strcmp(mysql_fetch_row(res)[0], "0") != 0;
        mysql_free_result(res);
        if (!exists)
            break;
    }
    sprintf(q, "insert into `sessions` values('%.128s', %d, null);",
            sid, epoch_time() + SECS_PER_DAY);
    create_query_result(logname, mysql, q, false);
    return sid;
}

void sha512sum(const char *path, char *res)
{
    char *cmd = new (std::nothrow) char[snprintf(NULL, 0, "sha512sum '%s'", path) + 1];
    sprintf(cmd, "sha512sum '%s'", path);
    FILE *fp = popen(cmd, "r");
    fscanf(fp, "%s", res);
    pclose(fp);
    delete[] cmd;
}

void rremove(const char *path)
{
    char *cmd = new (std::nothrow) char[snprintf(NULL, 0, "rm -rf %s", path) + 1];
    sprintf(cmd, "rm -rf '%s'", path);
    FILE *fp = popen(cmd, "r");
    pclose(fp);
    delete[] cmd;
}

bool sha512valid(const char *sha)
{
    for (const char *p = sha; p < sha + 128; p++)
        if (!((*p <= '9' && *p >= '0') ||
              (*p <= 'F' && *p >= 'A') ||
              (*p <= 'f' && *p >= 'a')))
            return false;
    return true;
}

int construct_msg(write_queue_t *p, const char *msg)
{
    p->size = 144;
    memcpy(p->buf, "message", strlen("message"));
    memcpy(p->buf + 16, msg, strlen(msg));
    return 0;
}

int mode_bits(const char *filepath)
{
    struct stat s;
    if (stat(filepath, &s) == -1)
        return 0;
    return s.st_mode;
}

int processincoming(
    const char *logname, const char *rootpath,
    MYSQL *mysql, const char *raw,
    write_queue_t *rear, sock_inet_base *sock)
{
    LOGTIME(logname, "Get request '%.16s'\n", raw);
    if (STR_EQ(raw, "login"))
    {
        const char *username = raw + 16, *passwd = username + 64;
        (rear = rear->next = new (std::nothrow) write_queue_t)->next = NULL;
        rear->sock = sock;
        memset(rear->buf, 0, sizeof(rear->buf));
        char q[128] = {0};
        sprintf(q, "select `userid`, `passwd` from user where `username` = '%.64s';", username);
        MYSQL_RES *result = create_query_result(logname, mysql, q, true);
        MYSQL_ROW row = mysql_fetch_row(result);
        if (!row)
            construct_msg(rear, MSG_NOUSER);
        else if (strncmp(row[1], passwd, 64) != 0)
            construct_msg(rear, MSG_WPSWD);
        else
        {
            char sid[129];
            rear->size = 144;
            memcpy(rear->buf, "login", strlen("login"));
            memcpy(rear->buf + 16, newsid(logname, mysql, sid), 128);
            char qinsert[128] = {0};
            sprintf(qinsert, "insert into `login` values('%.128s', '%.16s');", sid, row[0]);
            create_query_result(logname, mysql, qinsert, false);
        }
        mysql_free_result(result);
    }
    else if (STR_EQ(raw, "register"))
    {
        const char *username = raw + 16, *passwd = username + 64, *invcode = passwd + 64;
        (rear = rear->next = new (std::nothrow) write_queue_t)->next = NULL;
        rear->sock = sock;
        memset(rear->buf, 0, sizeof(rear->buf));
        char q[128] = {0};
        sprintf(q, "select count(*), maxnum from `invcode` natural join `user` where `invcode` = '%.8s'",
                invcode);
        MYSQL_RES *result = create_query_result(logname, mysql, q, true);
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row && strtol(row[0], NULL, 10) < strtol(row[1], NULL, 10))
        {
            rear->size = 144;
            char qinsert[128] = {0};
            sprintf(qinsert, "select username from user where `username` = '%.64s';", username);
            MYSQL_RES *rinsert = create_query_result(logname, mysql, qinsert, true);
            if (mysql_fetch_row(rinsert))
                construct_msg(rear, MSG_UEXST);
            else
            {
                sprintf(qinsert, "insert into `user`(`username`, `passwd`, `invcode`) \
                    values ('%s', '%s', '%s');",
                        username, passwd, invcode);
                create_query_result(logname, mysql, qinsert, false);
                memcpy(rear->buf, "register", strlen("register"));
                memcpy(rear->buf + 16, "Success.", strlen("Success."));
            }
            mysql_free_result(rinsert);
        }
        else
            construct_msg(rear, MSG_WINV);
        mysql_free_result(result);
    }
    else if (STR_EQ(raw, "download"))
    {
        (rear = rear->next = new (std::nothrow) write_queue_t)->next = NULL;
        rear->sock = sock;
        memset(rear->buf, 0, sizeof(rear->buf));
        const char *sid = raw + 16, *pathlenstr = sid + 128, *path = pathlenstr + 16;
        if (pathlenstr[15] || pathlenstr[0] > '9' || pathlenstr[0] < '0')
            return construct_msg(rear, MSG_UNREC);
        const long pathlen = strtol(pathlenstr, NULL, 10);
        char q[256] = {0};
        sprintf(q, "select `userid` from `login` where `sessionid` = '%.128s'", sid);
        MYSQL_RES *res = create_query_result(logname, mysql, q, true);
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row)
        {
            const char *uid = row[0];
            int fullpathlen = snprintf(NULL, 0, "%s/%s%.*s", rootpath, uid, pathlen, path);
            char *fullpath = new (std::nothrow) char[fullpathlen + 1];
            snprintf(fullpath, fullpathlen + 1, "%s/%s%.*s", rootpath, uid, pathlen, path);
            char *rpath = realpath(fullpath, NULL);
            if (rpath) // file or directory exist
            {
                struct stat statbuf;
                stat(rpath, &statbuf);
                rear->size = 160;
                memcpy(rear->buf, "download", strlen("download"));
                if (S_ISDIR(statbuf.st_mode))
                {
                    // create a information file in rootpath/d/
                    int infopathlen = snprintf(NULL, 0, "%s/d/new", rootpath);
                    char *infopath = new (std::nothrow) char[infopathlen + 1];
                    snprintf(infopath, infopathlen + 1, "%s/d/new", rootpath);
                    FILE *fp = fopen(infopath, "w");
                    DIR *dirp = opendir(rpath);
                    struct dirent *dir;
                    while (dir = readdir(dirp))
                    {
                        int subpathlen = snprintf(NULL, 0, "%s/%s", rpath, dir->d_name);
                        char *subpath = new (std::nothrow) char[subpathlen + 1];
                        snprintf(subpath, subpathlen + 1, "%s/%s", rpath, dir->d_name);
                        stat(subpath, &statbuf);
                        fprintf(fp, "%s %s %llu\n", dir->d_name,
                                S_ISDIR(statbuf.st_mode) ? "d" : "r", statbuf.st_size);
                        delete[] subpath;
                    }
                    closedir(dirp);
                    fclose(fp);
                    char sha512name[129];
                    sha512sum(infopath, sha512name);
                    int newpathlen = snprintf(NULL, 0, "%s/d/%s", rootpath, sha512name);
                    char *newpath = new (std::nothrow) char[newpathlen + 1];
                    snprintf(newpath, newpathlen + 1, "%s/d/%s", rootpath, sha512name);
                    rename(infopath, newpath);
                    stat(newpath, &statbuf);
                    memcpy(rear->buf + 16, sha512name, 128);
                    delete[] infopath;
                    delete[] newpath;
                }
                else
                {
                    const char *name = basename(rpath);
                    memcpy(rear->buf + 16, name, strlen(name));
                }
                snprintf(rear->buf + 144, 16, "%llu", statbuf.st_size);
                free(rpath);
            }
            else // file not found
                construct_msg(rear, MSG_NFILE);
            delete[] fullpath;
        }
        else // session not found
            construct_msg(rear, MSG_NSESS);
        mysql_free_result(res);
    }
    else if (STR_EQ(raw, "downdata"))
    {
        const char *sha = raw + 16, *off_str = sha + 128, *len_str = off_str + 16;
        (rear = rear->next = new (std::nothrow) write_queue_t)->next = NULL;
        rear->sock = sock;
        memset(rear->buf, 0, sizeof(rear->buf));
        if (off_str[15] || off_str[0] > '9' || off_str[0] < '0' ||
            len_str[15] || len_str[0] > '9' || len_str[0] < '0')
            return construct_msg(rear, MSG_UNREC);
        const size_t offset = strtoull(off_str, NULL, 10), len = strtoull(len_str, NULL, 10);
        rear->size = 16 + len;
        if (rear->size > MSG_BUF_SZ)
            return construct_msg(rear, MSG_TOOLRG);
        int l = snprintf(NULL, 0, "%s/d/%.128s", rootpath, sha);
        char *fullpath = new (std::nothrow) char[l + 1];
        snprintf(fullpath, l + 1, "%s/d/%.128s", rootpath, sha);
        if (mode_bits(fullpath) & S_IREAD ||
            mode_bits((fullpath[strlen(rootpath) + 1] = 'r', fullpath)) & S_IREAD)
        {
            rear->size = 16 + len;
            memcpy(rear->buf, "downdata", strlen("downdata"));
            FILE *fp = fopen(fullpath, "rb");
            fseek(fp, offset, SEEK_SET);
            fread(rear->buf + 16, 1, len, fp);
            fclose(fp);
        }
        else
            construct_msg(rear, MSG_NFILE);
        delete[] fullpath;
    }
    else if (STR_EQ(raw, "upload"))
    {
        const char *sid = raw + 16, *sha = sid + 128, *filesz_str = sha + 128,
                   *pathlen_str = filesz_str + 16, *path = pathlen_str + 16;
        (rear = rear->next = new (std::nothrow) write_queue_t)->next = NULL;
        rear->sock = sock;
        memset(rear->buf, 0, sizeof(rear->buf));
        if (filesz_str[15] || filesz_str[0] > '9' || filesz_str[0] < '0' ||
            pathlen_str[15] || pathlen_str[0] > '9' || pathlen_str[0] < '0')
            return construct_msg(rear, MSG_UNREC);
        const size_t filesz = strtoull(filesz_str, NULL, 10), pathlen = strtoull(pathlen_str, NULL, 10);
        char q[256] = {0};
        sprintf(q, "select `userid` from `login` where `sessionid` = '%.128s'", sid);
        MYSQL_RES *res = create_query_result(logname, mysql, q, true);
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row)
        {
            const char *uid = row[0];
            int fullpathlen = snprintf(NULL, 0, "%s/%s%.*s", rootpath, uid, pathlen, path);
            int rpathlen = snprintf(NULL, 0, "%s/r/%.128s", rootpath, sha);
            char *fullpath = new (std::nothrow) char[fullpathlen + 1];
            char *rpath = new (std::nothrow) char[rpathlen + 1];
            snprintf(fullpath, fullpathlen + 1, "%s/%s%.*s", rootpath, uid, pathlen, path);
            snprintf(rpath, rpathlen + 1, "%s/r/%.128s", rootpath, sha);
            if (sha512valid(sha))
            {
                FILE *fp;
                bool suc = access(rpath, F_OK) == 0;
                if (!suc && (fp = fopen(rpath, "w")))
                {
                    suc = true;
                    fclose(fp);
                    chmod(rpath, 0000);
                    char q[256] = {};
                    sprintf(q, "insert into `upload` values('%.128s', 0, %lld, 'pending')",
                            sha, filesz - 1);
                    create_query_result(logname, mysql, q, false);
                }
                if (suc)
                    unlink(fullpath);
                if (suc && symlink(rpath, fullpath) == 0)
                {
                    rear->size = 32;
                    memcpy(rear->buf, "upload", strlen("upload"));
                    memcpy(rear->buf + 16, "pending", strlen("pending"));
                }
                else
                    construct_msg(rear, MSG_ECREAT);
            }
            else if (mkdir(fullpath, 0755) == -1)
                construct_msg(rear, MSG_ECREAT);
            else
            {
                rear->size = 32;
                memcpy(rear->buf, "upload", strlen("upload"));
                memcpy(rear->buf + 16, "pending", strlen("pending"));
            }
            delete[] fullpath;
            delete[] rpath;
        }
        else
            construct_msg(rear, MSG_NSESS);
        mysql_free_result(res);
    }
    else if (STR_EQ(raw, "upready"))
    {
        (rear = rear->next = new (std::nothrow) write_queue_t)->next = NULL;
        rear->sock = sock;
        memset(rear->buf, 0, sizeof(rear->buf));
        const char *sha = raw + 16;
        char q[256] = {0};
        sprintf(q, "select `begin`, `end` from `upload` \
            where `fileid` = '%.128s' and `status` = 'pending'",
                sha);
        MYSQL_RES *res = create_query_result(logname, mysql, q, true);
        MYSQL_ROW row = mysql_fetch_row(res);
        rear->size = 48;
        memcpy(rear->buf, "upready", strlen("upready"));
        if (row)
        {
            const size_t b = strtoull(row[0], NULL, 10), e = strtoull(row[1], NULL, 10);
            snprintf(rear->buf + 16, 16, "%llu", b);
            size_t sz = e - b + 1;
            if (sz > MSG_BUF_SZ - 176)
            {
                sz = MSG_BUF_SZ - 176;
                sprintf(q, "insert into `upload` values('%.128s', %llu, %llu, 'uploading')",
                        sha, b, b + sz - 1);
                create_query_result(logname, mysql, q, false);
                sprintf(q, "update `upload` set `begin` = %llu \
                    where `fileid` = '%.128s' and `begin` = %llu and `end` = %llu",
                        b + sz, sha, b, e);
                create_query_result(logname, mysql, q, false);
            }
            else
            {
                sprintf(q, "update `upload` set `status` = 'uploading' \
                    where `fileid` = %.128s and `begin` = %llu and `end` = %llu",
                        sha, b, e);
                create_query_result(logname, mysql, q, false);
            }
            snprintf(rear->buf + 32, 16, "%llu", sz);
            char q[256] = {0};
        }
        else
        {
            snprintf(rear->buf + 16, 16, "%llu", 0);
            snprintf(rear->buf + 32, 16, "%llu", 0);
        }
        mysql_free_result(res);
    }
    else if (STR_EQ("updata", raw))
    {
        const char *sha = raw + 16, *off_str = sha + 128,
                   *len_str = off_str + 16, *data = len_str + 16;
        (rear = rear->next = new (std::nothrow) write_queue_t)->next = NULL;
        rear->sock = sock;
        memset(rear->buf, 0, sizeof(rear->buf));
        if (off_str[15] || off_str[0] > '9' || off_str[0] < '0' ||
            len_str[15] || len_str[0] > '9' || len_str[0] < '0')
            return construct_msg(rear, MSG_UNREC);
        const size_t offset = strtoull(off_str, NULL, 10), len = strtoull(len_str, NULL, 10);
        char q[256] = {0};
        if (offset >= 0 && len > 0)
        {
            size_t b = offset, e = offset + len - 1;
            sprintf(q, "select `begin` from `upload` \
                where `fileid` = '%.128s' and `begin` = %llu \
                and `end` = %llu and `status` = 'uploading'",
                    sha, b, e);
            MYSQL_RES *res = create_query_result(logname, mysql, q, true);
            if (mysql_fetch_row(res) == NULL)
                return mysql_free_result(res), construct_msg(rear, MSG_NUPLD);
            mysql_free_result(res);
            int l = snprintf(NULL, 0, "%s/r/%.128s", rootpath, sha);
            char *fullpath = new (std::nothrow) char[l + 1];
            snprintf(fullpath, l + 1, "%s/r/%.128s", rootpath, sha);
            FILE *fp = fopen(fullpath, "rb+");
            fseek(fp, offset, SEEK_SET);
            fwrite(data, 1, len, fp);
            fclose(fp);
            sprintf(q, "delete from `upload` where \
                `fileid` = '%.128s' and `begin` = %llu and `end` = %llu",
                    sha, b, e);
            create_query_result(logname, mysql, q, false);
            sprintf(q, "select `status` from `upload` where `fileid` = '%.128s'", sha);
            res = create_query_result(logname, mysql, q, true);
            if (mysql_num_rows(res) == 0)
                chmod(fullpath, 0644);
            mysql_free_result(res);
            delete[] fullpath;
        }
        int l = snprintf(NULL, 0, "%s/r/%.128s", rootpath, sha);
        char *fullpath = new (std::nothrow) char[l + 1];
        snprintf(fullpath, l + 1, "%s/r/%.128s", rootpath, sha);
        bool readable = mode_bits(fullpath) & S_IREAD;
        char realsha[128];
        if (readable)
            sha512sum(fullpath, realsha);
        if (readable && strncmp(sha, realsha, 128) != 0)
        {
            construct_msg(rear, MSG_WSHA);
            remove(fullpath);
        }
        else
        {
            rear->size = 32;
            memcpy(rear->buf, "updata", 16);
            memcpy(rear->buf + 16, readable ? "done" : "pending",
                   strlen(readable ? "done" : "pending"));
        }
        delete[] fullpath;
    }
    else if (STR_EQ(raw, "delete"))
    {
        (rear = rear->next = new (std::nothrow) write_queue_t)->next = NULL;
        rear->sock = sock;
        memset(rear->buf, 0, sizeof(rear->buf));
        const char *sid = raw + 16, *pathlenstr = sid + 128, *path = pathlenstr + 16;
        if (pathlenstr[15] || pathlenstr[0] > '9' || pathlenstr[0] < '0')
            return construct_msg(rear, MSG_UNREC);
        const long pathlen = strtol(pathlenstr, NULL, 10);
        char q[256] = {0};
        sprintf(q, "select `userid` from `login` where `sessionid` = '%.128s'", sid);
        MYSQL_RES *res = create_query_result(logname, mysql, q, true);
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row)
        {
            const char *uid = row[0];
            int fullpathlen = snprintf(NULL, 0, "%s/%s%.*s", rootpath, uid, pathlen, path);
            char *fullpath = new (std::nothrow) char[fullpathlen + 1];
            snprintf(fullpath, fullpathlen + 1, "%s/%s%.*s", rootpath, uid, pathlen, path);
            rremove(fullpath);
            delete[] fullpath;
            rear->size = 32;
            memcpy(rear->buf, "delete", strlen("delete"));
            memcpy(rear->buf + 16, "success", strlen("success"));
        }
        else // session not found
            construct_msg(rear, MSG_NSESS);
        mysql_free_result(res);
    }
    else
    {
        (rear = rear->next = new (std::nothrow) write_queue_t)->next = NULL;
        rear->sock = sock;
        memset(rear->buf, 0, sizeof(rear->buf));
        construct_msg(rear, MSG_UNREC);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // process arguments
    const char *options[] = {"--daemon", "--port", "--logname", "--rootpath", "--help"};
    int idx[5];
    my_args(argc, argv, 5, options, idx);
    bool daemon = arg_exist(argc, argv, idx[0]);
    int port = arg_int32(argc, argv, idx[1] + 1);
    char *logname = arg_str(argc, argv, idx[2] + 1);
    char *rootpath = arg_str(argc, argv, idx[3] + 1);
    bool info = arg_exist(argc, argv, idx[4]);
    if (port & 0xffff0000 || rootpath == NULL)
        fprintf(stderr, "Invalid argument.\n"), info = true;
    if (info)
    {
        printf("A simple cloud drive server. Arguments:\n");
        printf("    --daemon       Run as daemon.\n");
        printf("    --port PORT    Listen on port PORT.\n");
        printf("    --logname S    Store log in the path S.\n");
        printf("    --rootpath P   Specify file root P.\n");
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
    write_queue_t front = {0, {0}, NULL, NULL}, *rear = &front;

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
                    if (size > 0)
                        processincoming(logname, rootpath, mysql, buf, rear, p->sock);
                    else
                    {
                        // connection is closed by any side of connection
                        pre->next = p->next;
                        p->sock->set_epoll(epfd, EPOLL_CTL_DEL);
                        logonclose(logname, p->sock);
                        for (write_queue_t *pre = &front; pre && pre->next; pre = pre->next)
                            if (pre->next->sock == p->sock)
                            {
                                if (rear == pre->next)
                                    rear = pre; // update rear_q if removing the last node
                                write_queue_t *p = pre->next;
                                pre->next = p->next;
                                delete p;
                            }
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
