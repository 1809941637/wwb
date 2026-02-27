#ifndef COMMON_H
#define COMMON_H


#ifdef __cplusplus
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include "cjson/cJSON.h"
#include <sys/time.h>
#include "log_comm.h"
#include <pthread.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {  
#endif


#define TRRO_WEBRTC_CONFIG_PATH   "../config/config.json"
#define TRRO_WEBRTC_LICENSE_PATH  "../config/license.txt"

#define MOS_COMM_CONFIG_PATH  "../config/discovery_config.json"

#define FRAME_PARAM_CONFIG_PATH "../config/frame_param.json"


#define F_FRAME_TOPIC   "VIN/F_H264"
#define FN_FRAME_TOPIC  "VIN/FN_H264"
#define B_FRAME_TOPIC   "VIN/B_H264"
#define LF_FRAME_TOPIC  "VIN/LF_H264"
#define RF_FRAME_TOPIC  "VIN/RF_H264"
#define LB_FRAME_TOPIC  "VIN/LB_H264"
#define RB_FRAME_TOPIC  "VIN/RB_H264"
#define AVM_FRAME_TOPIC  "preview"
#define CAN_GATEWAY_PUB_TOPIC "CAN/Tx"
#define CAN_GATEWAY_SUB_TOPIC "CAN/Rx"

// 安全的 free（防止重复释放）
#define SAFE_FREE(p) do { if (p) { free(p); p = NULL; } } while(0)

// 安全的 delete（C++）
#ifdef __cplusplus
    #define SAFE_DELETE(p) do { if (p) { delete (p); p = nullptr; } } while(0)
    #define SAFE_DELETE_ARRAY(p) do { if (p) { delete[] (p); p = nullptr; } } while(0)
#endif





#ifdef __cplusplus
} 
#endif

#endif /* COMMON_H */
