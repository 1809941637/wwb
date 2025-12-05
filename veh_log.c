#include "veh_log.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

/* 日志文件路径与大小（32M） */
#define VEH_LOG_FILE_PATH   "/sdcard/vehinfo.log"
#define VEH_LOG_FILE_SIZE   (32 * 1024 * 1024) /* 32MB */

/* 元数据魔数和版本，用于校验 */
#define VEH_LOG_META_MAGIC  0x5645484C  /* 'VEHL' */
#define VEH_LOG_META_VER    1

typedef struct veh_log_meta_s
{
    uint32_t magic;
    uint32_t version;
    uint64_t pos;      /* 写指针，相对于数据区起始位置 */
} veh_log_meta_t;

/* 在日志文件头部预留与元数据等长的空间 */
#define VEH_LOG_META_SIZE   ((off_t)sizeof(veh_log_meta_t))

static int          s_log_fd   = -1;
/* s_log_pos 是逻辑写指针，相对于数据区起始（0 ~ VEH_LOG_FILE_SIZE - VEH_LOG_META_SIZE - 1） */
static off_t        s_log_pos  = 0;
static pthread_mutex_t s_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int          s_enable_file_log = 0; /* 是否写入文件的开关 */

/* 内部函数声明 */
static int  veh_log_open_or_create(void);
static void veh_log_append(const char *buf, size_t len);
static void veh_log_format_time(char *buf, size_t buf_len);
static int  veh_log_load_meta(void);
static void veh_log_save_meta(void);

/* 初始化日志模块：检查/创建固定大小文件，并设置写入位置
 * enable_file_log = 0  只打印，不写文件
 * enable_file_log !=0 打印 + 写文件
 */
int veh_log_init(int enable_file_log)
{
    pthread_mutex_lock(&s_log_mutex);
    s_enable_file_log = (enable_file_log != 0);

    /* 如果不需要写文件，直接返回即可（仍保留 printf 功能） */
    if (!s_enable_file_log)
    {
        if (s_log_fd >= 0)
        {
            close(s_log_fd);
            s_log_fd = -1;
            s_log_pos = 0;
        }
        pthread_mutex_unlock(&s_log_mutex);
        return 0;
    }

    if (s_log_fd >= 0)
    {
        pthread_mutex_unlock(&s_log_mutex);
        return 0; /* 已初始化过，且保持当前文件句柄与位置 */
    }

    if (veh_log_open_or_create() != 0)
    {
        pthread_mutex_unlock(&s_log_mutex);
        return -1;
    }

    pthread_mutex_unlock(&s_log_mutex);
    return 0;
}

/* 关闭日志模块（可选） */
void veh_log_deinit(void)
{
    pthread_mutex_lock(&s_log_mutex);
    s_enable_file_log = 0;
    if (s_log_fd >= 0)
    {
        close(s_log_fd);
        s_log_fd = -1;
        s_log_pos = 0;
    }
    pthread_mutex_unlock(&s_log_mutex);
}

/* 对外日志写接口：打印到标准输出，并写入循环日志文件 */
void veh_log_write(veh_log_level_t level,
                   const char *tag,
                   const char *file,
                   int line,
                   const char *fmt, ...)
{
    char time_str[64];
    char msg_buf[1024];
    char final_buf[1400];
    const char *level_str = "INFO";

    if (tag == NULL)
    {
        tag = "";
    }

    switch (level)
    {
    case VEH_LOG_LEVEL_WARN:
        level_str = "WARN";
        break;
    case VEH_LOG_LEVEL_ERROR:
        level_str = "ERROR";
        break;
    case VEH_LOG_LEVEL_INFO:
    default:
        level_str = "INFO";
        break;
    }

    veh_log_format_time(time_str, sizeof(time_str));

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    /* 从完整路径中提取文件名（只保留文件名，不包含路径） */
    const char *file_name = file ? file : "";
    if (file && *file)
    {
        const char *last_slash = strrchr(file, '/');
        const char *last_backslash = strrchr(file, '\\');
        const char *last_sep = (last_slash > last_backslash) ? last_slash : last_backslash;
        if (last_sep)
        {
            file_name = last_sep + 1;
        }
        else
        {
            file_name = file;
        }
    }

    /* 构造最终输出：时间戳 + 等级 + 自定义tag + 文件名和行号 + 实际内容 */
    snprintf(final_buf, sizeof(final_buf),
             "[%s][%s][%s][%s:%d] %s",
             time_str,
             level_str,
             tag,
             file_name,
             line,
             msg_buf);

    /* 输出到标准输出 */
    fputs(final_buf, stdout);
    fflush(stdout);

    /* 写入到循环日志文件（受开关控制） */
    pthread_mutex_lock(&s_log_mutex);
    if (s_enable_file_log)
    {
        if (s_log_fd < 0)
        {
            /* 如果日志开关打开但还没成功打开文件，尝试打开一次 */
            veh_log_open_or_create();
        }
        if (s_log_fd >= 0)
        {
            veh_log_append(final_buf, strlen(final_buf));
        }
    }
    pthread_mutex_unlock(&s_log_mutex);
}

/* 打开或创建固定大小的日志文件 */
static int veh_log_open_or_create(void)
{
    struct stat st;
    int exists = (stat(VEH_LOG_FILE_PATH, &st) == 0);

    if (exists)
    {
        /* 已存在：继续写入（不丢弃原数据） */
        s_log_fd = open(VEH_LOG_FILE_PATH, O_RDWR);
        if (s_log_fd < 0)
        {
            return -1;
        }

        if (st.st_size < VEH_LOG_FILE_SIZE)
        {
            /* 扩展到固定大小 */
            if (ftruncate(s_log_fd, VEH_LOG_FILE_SIZE) != 0)
            {
                /* 如果扩展失败也尽量继续用现有大小 */
            }
        }
        else if (st.st_size > VEH_LOG_FILE_SIZE)
        {
            /* 如果文件超过预期，截断到固定大小 */
            (void)ftruncate(s_log_fd, VEH_LOG_FILE_SIZE);
        }

        if (veh_log_load_meta() != 0)
        {
            /* 元数据无效，重置指针 */
            s_log_pos = 0;
            veh_log_save_meta();
        }
    }
    else
    {
        /* 不存在：创建并预分配到32M */
        s_log_fd = open(VEH_LOG_FILE_PATH,
                        O_RDWR | O_CREAT,
                        0644);
        if (s_log_fd < 0)
        {
            return -1;
        }

        if (ftruncate(s_log_fd, VEH_LOG_FILE_SIZE) != 0)
        {
            /* 理论上不应失败，失败也继续使用当前大小，只是无法保证刚好32M */
        }

        s_log_pos = 0;
        veh_log_save_meta();
    }

    /* 设置文件偏移到当前写位置（实际偏移 = 元数据长度 + 逻辑写指针） */
    off_t data_start = VEH_LOG_META_SIZE;
    off_t file_off   = data_start + s_log_pos;
    if (lseek(s_log_fd, file_off, SEEK_SET) < 0)
    {
        s_log_pos = 0;
        (void)lseek(s_log_fd, data_start, SEEK_SET);
    }

    return 0;
}

/* 将数据写入循环日志文件（不会超过32M，采用覆盖最老数据方式） */
static void veh_log_append(const char *buf, size_t len)
{
    if (s_log_fd < 0 || buf == NULL || len == 0)
    {
        return;
    }

    const off_t data_start = VEH_LOG_META_SIZE;
    const off_t data_size  = VEH_LOG_FILE_SIZE - VEH_LOG_META_SIZE;

    if (data_size <= 0)
    {
        return;
    }

    /* 单条日志如果大于数据区总容量，只保留最后 data_size 字节 */
    if ((off_t)len >= data_size)
    {
        buf += (len - (size_t)data_size);
        len  = (size_t)data_size;
    }

    if (s_log_pos + (off_t)len <= data_size)
    {
        /* 不需要回绕覆盖 */
        ssize_t w = write(s_log_fd, buf, len);
        (void)w;
        s_log_pos += (off_t)len;
        if (s_log_pos >= data_size)
        {
            s_log_pos = 0;
            (void)lseek(s_log_fd, data_start, SEEK_SET);
        }
    }
    else
    {
        /* 需要从尾部写一部分，再从头写剩余部分 */
        size_t first  = (size_t)(data_size - s_log_pos);
        size_t second = len - first;

        if (first > 0)
        {
            ssize_t w1 = write(s_log_fd, buf, first);
            (void)w1;
        }

        (void)lseek(s_log_fd, data_start, SEEK_SET);

        if (second > 0)
        {
            ssize_t w2 = write(s_log_fd, buf + first, second);
            (void)w2;
        }

        s_log_pos = (off_t)second;
    }

    /* 写指针更新后，刷新元数据 */
    veh_log_save_meta();
}

/* 获取当前时间字符串（精确到毫秒） */
static void veh_log_format_time(char *buf, size_t buf_len)
{
    struct timeval tv;
    struct tm tm_info;
    gettimeofday(&tv, NULL);

    localtime_r(&tv.tv_sec, &tm_info);

    /* 格式：YYYY-MM-DD HH:MM:SS.mmm */
    snprintf(buf, buf_len,
             "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             tm_info.tm_year + 1900,
             tm_info.tm_mon + 1,
             tm_info.tm_mday,
             tm_info.tm_hour,
             tm_info.tm_min,
             tm_info.tm_sec,
             tv.tv_usec / 1000);
}


static int veh_log_load_meta(void)
{
    veh_log_meta_t meta;
    ssize_t r;

    if (s_log_fd < 0)
    {
        return -1;
    }

    r = pread(s_log_fd, &meta, sizeof(meta), 0);
    if (r != (ssize_t)sizeof(meta))
    {
        return -1;
    }

    if (meta.magic != VEH_LOG_META_MAGIC || meta.version != VEH_LOG_META_VER)
    {
        return -1;
    }

    off_t data_size = VEH_LOG_FILE_SIZE - VEH_LOG_META_SIZE;
    if (data_size <= 0)
    {
        return -1;
    }

    if ((off_t)meta.pos < 0 || (off_t)meta.pos >= data_size)
    {
        return -1;
    }

    s_log_pos = (off_t)meta.pos;
    return 0;
}


static void veh_log_save_meta(void)
{
    veh_log_meta_t meta;

    if (s_log_fd < 0)
    {
        return;
    }

    meta.magic   = VEH_LOG_META_MAGIC;
    meta.version = VEH_LOG_META_VER;
    meta.pos     = (uint64_t)s_log_pos;

    (void)pwrite(s_log_fd, &meta, sizeof(meta), 0);
}

