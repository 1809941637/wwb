#include "../include/log_comm.h"
#include <sys/time.h>
#include <errno.h>


// 当前日志级别（默认输出INFO及以上级别）
static log_level_t g_log_level = LOG_LEVEL_INFO;

// 获取日志级别字符串
const char* log_get_level_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return LOG_LEVEL_STR_DEBUG;
        case LOG_LEVEL_INFO:  return LOG_LEVEL_STR_INFO;
        case LOG_LEVEL_WARN:  return LOG_LEVEL_STR_WARN;
        case LOG_LEVEL_ERROR: return LOG_LEVEL_STR_ERROR;
        case LOG_LEVEL_FATAL: return LOG_LEVEL_STR_FATAL;
        default: return "UNKNOWN";
    }
}

// 获取当前线程名
void log_get_thread_name(char* thread_name, size_t size) {
    if (thread_name == NULL || size == 0) {
        return;
    }
    
    // 尝试使用pthread_getname_np获取线程名（Linux特有）
    #ifdef __linux__
    int ret = pthread_getname_np(pthread_self(), thread_name, size);
    if (ret == 0 && thread_name[0] != '\0') {
        return;
    }
    #endif
    
    // 如果获取失败，使用线程ID作为标识
    unsigned long tid = (unsigned long)pthread_self();
    snprintf(thread_name, size, "T%lu", tid);
}

// 获取文件名（从完整路径中提取）
const char* log_get_filename(const char* filepath) {
    if (filepath == NULL) {
        return "unknown";
    }
    
    const char* filename = strrchr(filepath, '/');
    if (filename != NULL) {
        return filename + 1;  // 跳过 '/'
    }
    
    // Windows路径分隔符
    filename = strrchr(filepath, '\\');
    if (filename != NULL) {
        return filename + 1;  // 跳过 '\\'
    }
    
    return filepath;  // 没有路径分隔符，直接返回原字符串
}

// 日志输出函数
void log_output(log_level_t level, const char* file, int line, const char* format, ...) {
    // 检查日志级别
    if (level < g_log_level) {
        return;
    }
    
    // 获取当前时间（精确到毫秒）
    struct timeval tv;
    struct tm* tm_info;
    char time_str[64];
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    // 格式化时间戳: YYYY-MM-DD HH:MM:SS.mmm
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // 获取线程名
    char thread_name[32];
    log_get_thread_name(thread_name, sizeof(thread_name));
    
    // 获取文件名（不含路径）
    const char* filename = log_get_filename(file);
    
    // 获取日志级别字符串
    const char* level_str = log_get_level_string(level);
    
    // 打印日志头: [时间戳] [级别] [线程名] [文件名:行号]
    fprintf(stderr, "[%s.%03ld] [%s] [%s] [%s:%d] ", 
            time_str, tv.tv_usec / 1000, level_str, thread_name, filename, line);
    
    // 打印日志内容
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    // 换行
    //fprintf(stderr, "\n");
    
    // 刷新输出缓冲区，确保日志及时输出
    fflush(stderr);
}

// 设置日志级别
void log_set_level(log_level_t level) {
    g_log_level = level;
}

// 获取当前日志级别
log_level_t log_get_level(void) {
    return g_log_level;
}

// 初始化日志系统
void log_init(void) {
    // 可以在这里设置线程名
    #ifdef __linux__
    char thread_name[16];
    snprintf(thread_name, sizeof(thread_name), "vlaOrin");
    pthread_setname_np(pthread_self(), thread_name);
    #endif
    
    // 设置日志级别为INFO（默认）
    g_log_level = LOG_LEVEL_INFO;
}

