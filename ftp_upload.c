#include "ftp_upload.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

/* 内部工具函数声明 */
static int  ftp_connect_ctrl(const char *server, int port, int timeout_sec);
static int  ftp_read_reply(int sock, char *buf, size_t buf_len, int timeout_sec, int *code_out);
static int  ftp_send_cmd(int sock, const char *cmd, int timeout_sec);
static int  ftp_enter_pasv(int sock, int timeout_sec, char *pasv_ip, size_t ip_len, int *pasv_port);
static int  ftp_connect_data(const char *ip, int port, int timeout_sec);

/* 通用 FTP 上传实现（PASV 模式，二进制上传） */
int ftp_upload_file(const char *server,
                    int port,
                    const char *username,
                    const char *password,
                    const char *local_path,
                    const char *remote_path,
                    int timeout_sec)
{
    if (!server || !username || !password || !local_path || !remote_path)
    {
        return FTP_UPLOAD_ERR_PARAM;
    }

    if (port <= 0) port = 21;
    if (timeout_sec <= 0) timeout_sec = 10;

    int ctrl_sock = -1;
    int data_sock = -1;
    int local_fd  = -1;
    int ret = FTP_UPLOAD_OK;

    char buf[1024];
    int code = 0;

    /* 1. 连接控制通道 */
    ctrl_sock = ftp_connect_ctrl(server, port, timeout_sec);
    if (ctrl_sock < 0)
    {
        return FTP_UPLOAD_ERR_CONNECT_CTRL;
    }

    /* 2. 读取欢迎信息 (220) */
    if (ftp_read_reply(ctrl_sock, buf, sizeof(buf), timeout_sec, &code) < 0 || code != 220)
    {
        ret = FTP_UPLOAD_ERR_CTRL_REPLY;
        goto cleanup;
    }

    /* 3. 发送 USER */
    snprintf(buf, sizeof(buf), "USER %s\r\n", username);
    if (ftp_send_cmd(ctrl_sock, buf, timeout_sec) < 0 ||
        ftp_read_reply(ctrl_sock, buf, sizeof(buf), timeout_sec, &code) < 0)
    {
        ret = FTP_UPLOAD_ERR_AUTH;
        goto cleanup;
    }

    if (code == 331) /* 需要密码 */
    {
        /* 4. 发送 PASS */
        snprintf(buf, sizeof(buf), "PASS %s\r\n", password);
        if (ftp_send_cmd(ctrl_sock, buf, timeout_sec) < 0 ||
            ftp_read_reply(ctrl_sock, buf, sizeof(buf), timeout_sec, &code) < 0 ||
            (code != 230 && code != 202))
        {
            ret = FTP_UPLOAD_ERR_AUTH;
            goto cleanup;
        }
    }
    else if (code != 230 && code != 202)
    {
        ret = FTP_UPLOAD_ERR_AUTH;
        goto cleanup;
    }

    /* 5. 设置二进制模式 TYPE I */
    if (ftp_send_cmd(ctrl_sock, "TYPE I\r\n", timeout_sec) < 0 ||
        ftp_read_reply(ctrl_sock, buf, sizeof(buf), timeout_sec, &code) < 0 ||
        code != 200)
    {
        ret = FTP_UPLOAD_ERR_SET_BINARY;
        goto cleanup;
    }

    /* 6. 进入 PASV 模式，解析出数据连接 IP 和端口 */
    char pasv_ip[64] = {0};
    int  pasv_port   = 0;
    if (ftp_enter_pasv(ctrl_sock, timeout_sec, pasv_ip, sizeof(pasv_ip), &pasv_port) < 0)
    {
        ret = FTP_UPLOAD_ERR_PASV;
        goto cleanup;
    }

    /* 7. 建立数据连接 */
    data_sock = ftp_connect_data(pasv_ip, pasv_port, timeout_sec);
    if (data_sock < 0)
    {
        ret = FTP_UPLOAD_ERR_CONNECT_DATA;
        goto cleanup;
    }

    /* 8. 打开本地文件 */
    local_fd = open(local_path, O_RDONLY);
    if (local_fd < 0)
    {
        ret = FTP_UPLOAD_ERR_OPEN_LOCAL;
        goto cleanup;
    }

    /* 9. 发送 STOR 命令 */
    snprintf(buf, sizeof(buf), "STOR %s\r\n", remote_path);
    if (ftp_send_cmd(ctrl_sock, buf, timeout_sec) < 0 ||
        ftp_read_reply(ctrl_sock, buf, sizeof(buf), timeout_sec, &code) < 0 ||
        (code != 150 && code != 125))
    {
        ret = FTP_UPLOAD_ERR_STOR;
        goto cleanup;
    }

    /* 10. 通过数据通道发送文件内容 */
    while (1)
    {
        ssize_t r = read(local_fd, buf, sizeof(buf));
        if (r < 0)
        {
            ret = FTP_UPLOAD_ERR_SEND_DATA;
            goto cleanup;
        }
        else if (r == 0)
        {
            break; /* EOF */
        }

        ssize_t sent = 0;
        while (sent < r)
        {
            ssize_t w = send(data_sock, buf + sent, (size_t)(r - sent), 0);
            if (w <= 0)
            {
                ret = FTP_UPLOAD_ERR_SEND_DATA;
                goto cleanup;
            }
            sent += w;
        }
    }

    /* 11. 发送完毕，关闭数据通道，等待服务器完成回复 */
    if (data_sock >= 0)
    {
        close(data_sock);
        data_sock = -1;
    }

    if (ftp_read_reply(ctrl_sock, buf, sizeof(buf), timeout_sec, &code) < 0 ||
        (code != 226 && code != 250))
    {
        ret = FTP_UPLOAD_ERR_TRANSFER;
        goto cleanup;
    }

cleanup:
    /* 发送 QUIT，不强制检查返回值 */
    if (ctrl_sock >= 0)
    {
        ftp_send_cmd(ctrl_sock, "QUIT\r\n", timeout_sec);
        ftp_read_reply(ctrl_sock, buf, sizeof(buf), timeout_sec, &code);
    }

    if (data_sock >= 0)
    {
        close(data_sock);
    }
    if (local_fd >= 0)
    {
        close(local_fd);
    }
    if (ctrl_sock >= 0)
    {
        close(ctrl_sock);
    }

    return ret;
}

/* 建立 FTP 控制连接 */
static int ftp_connect_ctrl(const char *server, int port, int timeout_sec)
{
    char port_str[16];
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int sock = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(server, port_str, &hints, &res) != 0 || !res)
    {
        return FTP_UPLOAD_ERR_RESOLVE;
    }

    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next)
    {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0)
            continue;

        struct timeval tv;
        tv.tv_sec  = timeout_sec;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0)
        {
            break; /* success */
        }

        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);

    if (sock < 0)
    {
        return FTP_UPLOAD_ERR_CONNECT_CTRL;
    }

    return sock;
}

/* 读取一条 FTP 应答（可能包含多行），返回最后一行的 3 位状态码 */
static int ftp_read_reply(int sock, char *buf, size_t buf_len, int timeout_sec, int *code_out)
{
    if (!buf || buf_len == 0 || !code_out)
        return -1;

    *code_out = 0;
    buf[0] = '\0';

    char line[512];
    int code = 0;
    int first = 1;
    int multi = 0;

    while (1)
    {
        ssize_t n = recv(sock, line, sizeof(line) - 1, 0);
        if (n <= 0)
        {
            return -1;
        }
        line[n] = '\0';

        /* 简单拼接到 buf（可能截断，但不影响解析状态码） */
        strncat(buf, line, buf_len - strlen(buf) - 1);

        /* FTP 应答通常一行行返回，这里按行解析 */
        char *p = line;
        while (p && *p)
        {
            char *eol = strchr(p, '\n');
            if (eol)
            {
                *eol = '\0';
            }

            if (strlen(p) >= 3 &&
                p[0] >= '0' && p[0] <= '9' &&
                p[1] >= '0' && p[1] <= '9' &&
                p[2] >= '0' && p[2] <= '9')
            {
                int c = (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');

                if (first)
                {
                    code = c;
                    first = 0;
                    multi = (p[3] == '-'); /* 形如 "227-..." 表示多行应答 */
                }
                else if (!multi)
                {
                    code = c;
                }

                if (!multi && p[3] == ' ')
                {
                    /* 单行应答结束 */
                    *code_out = code;
                    return 0;
                }
                else if (multi && c == code && p[3] == ' ')
                {
                    /* 多行应答最后一行，以 "code " 开头 */
                    *code_out = code;
                    return 0;
                }
            }

            if (!eol)
                break;
            p = eol + 1;
        }
    }

    /* 不太会到这里 */
    return -1;
}

/* 发送一条 FTP 命令（已包含 \r\n 的字符串或普通命令） */
static int ftp_send_cmd(int sock, const char *cmd, int timeout_sec)
{
    (void)timeout_sec; /* 已通过 SO_SNDTIMEO 控制超时 */

    if (!cmd)
        return -1;

    size_t len = strlen(cmd);
    size_t sent = 0;

    while (sent < len)
    {
        ssize_t n = send(sock, cmd + sent, len - sent, 0);
        if (n <= 0)
            return -1;
        sent += n;
    }

    return 0;
}

/* 发送 PASV 并解析 227 应答中的 IP/端口 */
static int ftp_enter_pasv(int sock, int timeout_sec, char *pasv_ip, size_t ip_len, int *pasv_port)
{
    char buf[512];
    int code = 0;

    if (ftp_send_cmd(sock, "PASV\r\n", timeout_sec) < 0 ||
        ftp_read_reply(sock, buf, sizeof(buf), timeout_sec, &code) < 0 ||
        code != 227)
    {
        return -1;
    }

    /* 解析形如：227 Entering Passive Mode (h1,h2,h3,h4,p1,p2). */
    char *p = strchr(buf, '(');
    char *q = strchr(buf, ')');
    if (!p || !q || p >= q)
    {
        return -1;
    }

    int h1, h2, h3, h4, p1, p2;
    *q = '\0';
    if (sscanf(p + 1, "%d,%d,%d,%d,%d,%d",
               &h1, &h2, &h3, &h4, &p1, &p2) != 6)
    {
        return -1;
    }

    snprintf(pasv_ip, ip_len, "%d.%d.%d.%d", h1, h2, h3, h4);
    *pasv_port = p1 * 256 + p2;

    return 0;
}

/* 建立数据连接（PASV 返回的地址） */
static int ftp_connect_data(const char *ip, int port, int timeout_sec)
{
    if (!ip || port <= 0)
        return -1;

    char port_str[16];
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int sock = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(ip, port_str, &hints, &res) != 0 || !res)
    {
        return -1;
    }

    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next)
    {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0)
            continue;

        struct timeval tv;
        tv.tv_sec  = timeout_sec;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0)
        {
            break; /* success */
        }

        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);

    return sock;
}


