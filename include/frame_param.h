#ifndef FRAME_PARAM_H
#define FRAME_PARAM_H

#ifdef __cplusplus
extern "C" {
#endif


#define FRAME_TOPIC_MAX_LEN 64
#define FRAME_MAX_CHAN_NUM  16
#define FRAME_IP_MAX_LEN    32

typedef struct {
    char topic[FRAME_TOPIC_MAX_LEN];
    int  chan;
    int  width;
    int  height;
} frame_param_item_t;

typedef struct {
    char local_ip[FRAME_IP_MAX_LEN];
    int  local_port;
    int protocol_type; /*1-Net 0-shm*/

    int channum;
    frame_param_item_t items[FRAME_MAX_CHAN_NUM];
} frame_param_ctx_t;

/**
 * @brief  初始化并解析 frame_param.json
 * @param  json_path json 文件路径
 * @return 0 成功，-1 失败
 */
int frame_param_init(const char *json_path);

/**
 * @brief  根据 topic 获取 width 和 height
 * @param  topic 例如 "VIN/F_H264"
 * @param  width 输出
 * @param  height 输出
 * @return 0 成功，-1 未找到
 */
int frame_param_get(const char *topic, int *width, int *height);

/**
 * @brief  根据 chan 获取对应的 topic
 * @param  chan 通道号
 * @param  topic 输出的 topic 字符串
 * @param  topic_len topic 缓冲区长度
 * @return 0 成功，-1 未找到
 */
int frame_param_get_topic_by_chan(int chan, char *topic, int topic_len);

const char *frame_param_get_local_ip(void);

int frame_param_get_local_port(void);

int frame_param_get_protocol_type(void);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_PARAM_H */
