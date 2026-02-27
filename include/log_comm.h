#ifndef LOG_COMM_H
#define LOG_COMM_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// 日志级别定义
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_FATAL = 4
} log_level_t;

// 日志级别字符串
#define LOG_LEVEL_STR_DEBUG "DEBUG"
#define LOG_LEVEL_STR_INFO  "INFO "
#define LOG_LEVEL_STR_WARN  "WARN "
#define LOG_LEVEL_STR_ERROR "ERROR"
#define LOG_LEVEL_STR_FATAL "FATAL"

// 获取日志级别字符串
const char* log_get_level_string(log_level_t level);

// 获取当前线程名
void log_get_thread_name(char* thread_name, size_t size);

// 获取文件名（从完整路径中提取）
const char* log_get_filename(const char* filepath);

// 日志输出函数（内部使用）
void log_output(log_level_t level, const char* file, int line, const char* format, ...);

// 日志宏定义
// 格式: [时间戳] [级别] [线程名] [文件名:行号] 消息内容

#define LOG_BASE(level, format, ...) \
    do { \
        log_output(level, __FILE__, __LINE__, format, ##__VA_ARGS__); \
    } while(0)

// 各级别日志宏
#define DEBUG(format, ...) LOG_BASE(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define INFO(format, ...)  LOG_BASE(LOG_LEVEL_INFO,  format, ##__VA_ARGS__)
#define WARN(format, ...)  LOG_BASE(LOG_LEVEL_WARN,  format, ##__VA_ARGS__)
#define ERROR(format, ...) LOG_BASE(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define FATAL(format, ...) LOG_BASE(LOG_LEVEL_FATAL, format, ##__VA_ARGS__)

// 设置日志级别（低于此级别的日志不输出）
void log_set_level(log_level_t level);

// 获取当前日志级别
log_level_t log_get_level(void);

// 初始化日志系统（可选，设置线程名等）
void log_init(void);

#ifdef __cplusplus
}
#endif

#endif // LOG_COMM_H

