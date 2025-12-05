#ifndef FTP_UPLOAD_H
#define FTP_UPLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* FTP 上传返回码：
 *  0  成功
 * <0  失败（具体错误见定义）
 */
enum
{
    FTP_UPLOAD_OK                = 0,
    FTP_UPLOAD_ERR_PARAM         = -1,
    FTP_UPLOAD_ERR_RESOLVE       = -2,
    FTP_UPLOAD_ERR_CONNECT_CTRL  = -3,
    FTP_UPLOAD_ERR_CTRL_REPLY    = -4,
    FTP_UPLOAD_ERR_AUTH          = -5,
    FTP_UPLOAD_ERR_SET_BINARY    = -6,
    FTP_UPLOAD_ERR_PASV          = -7,
    FTP_UPLOAD_ERR_CONNECT_DATA  = -8,
    FTP_UPLOAD_ERR_OPEN_LOCAL    = -9,
    FTP_UPLOAD_ERR_STOR          = -10,
    FTP_UPLOAD_ERR_SEND_DATA     = -11,
    FTP_UPLOAD_ERR_TRANSFER      = -12,
};

/* 通用 FTP 文件上传接口
 *
 * server      : FTP 服务器地址 (域名或 IP)，如 "192.168.1.100"
 * port        : FTP 控制端口，通常为 21
 * username    : FTP 用户名
 * password    : FTP 密码
 * local_path  : 设备本地文件完整路径（要上传的文件）
 * remote_path : 远程文件路径（含文件名），例如 "logs/vehinfo.log"
 * timeout_sec : 连接和收发超时时间，单位秒，建议 5~30
 *
 * 返回值见上面的 FTP_UPLOAD_ERR_xxx 定义。
 */
int ftp_upload_file(const char *server,
                    int port,
                    const char *username,
                    const char *password,
                    const char *local_path,
                    const char *remote_path,
                    int timeout_sec);

#ifdef __cplusplus
}
#endif

#endif /* FTP_UPLOAD_H */


