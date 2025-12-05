#ifndef VEH_LOG_H
#define VEH_LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 日志等级 */
typedef enum
{
    VEH_LOG_LEVEL_INFO = 0,
    VEH_LOG_LEVEL_WARN,
    VEH_LOG_LEVEL_ERROR
} veh_log_level_t;

/* 初始化日志模块，程序启动时调用一次
 * enable_file_log = 0  只打印到终端，不写入文件
 * enable_file_log !=0 打印 + 写入 /sdcard/vehinfo.log
 */
int veh_log_init(int enable_file_log);

/* 反初始化（可选），程序退出前调用 */
void veh_log_deinit(void);

/* 底层日志输出接口，建议通过下面的宏使用 */
void veh_log_write(veh_log_level_t level,
                   const char *tag,
                   const char *file,
                   int line,
                   const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

/* 每个源码文件可自定义唯一 TAG，例如：
 *   #define VEH_LOG_TAG "ADAS"
 *   #include "veh_log.h"
 *
 * 如果未定义，则使用默认 TAG。
 */
#ifndef VEH_LOG_TAG
#define VEH_LOG_TAG "VEHINFO"
#endif

/* 对printf 的简单封装，支持等级/时间戳/文件名/行号等。
 * 使用方式：
 *   INFO("init ok %d", x);
 *   WARN("xxx");
 *   ERROR("failed: %d", err);
 */
#define INFO(fmt, ...)                                                          \
    veh_log_write(VEH_LOG_LEVEL_INFO,                                           \
                  VEH_LOG_TAG,                                                  \
                  __FILE__,                                                     \
                  __LINE__,                                                     \
                  fmt, ##__VA_ARGS__)

#define WARN(fmt, ...)                                                          \
    veh_log_write(VEH_LOG_LEVEL_WARN,                                           \
                  VEH_LOG_TAG,                                                  \
                  __FILE__,                                                     \
                  __LINE__,                                                     \
                  fmt, ##__VA_ARGS__)

#define ERROR(fmt, ...)                                                         \
    veh_log_write(VEH_LOG_LEVEL_ERROR,                                          \
                  VEH_LOG_TAG,                                                  \
                  __FILE__,                                                     \
                  __LINE__,                                                     \
                  fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* VEH_LOG_H */


