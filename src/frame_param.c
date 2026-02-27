#include "frame_param.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjson/cJSON.h"

static frame_param_ctx_t g_frame_param_ctx;

static char *json_file_read(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    fread(buf, 1, len, fp);
    buf[len] = '\0';

    fclose(fp);
    return buf;
}

int frame_param_init(const char *json_path)
{
    if (!json_path) {
        return -1;
    }

    memset(&g_frame_param_ctx, 0, sizeof(g_frame_param_ctx));

    char *json_buf = json_file_read(json_path);
    if (!json_buf) {
        return -1;
    }

    cJSON *root = cJSON_Parse(json_buf);
    free(json_buf);

    if (!root) {
        return -1;
    }

    cJSON *local_ip = cJSON_GetObjectItem(root, "local_ip");
    if (cJSON_IsString(local_ip) && local_ip->valuestring) {
        strncpy(g_frame_param_ctx.local_ip,
                local_ip->valuestring,
                FRAME_IP_MAX_LEN - 1);
    }

    cJSON *local_port = cJSON_GetObjectItem(root, "local_port");
    if (cJSON_IsNumber(local_port)) {
        g_frame_param_ctx.local_port = local_port->valueint;
    }

    cJSON *protocol_type = cJSON_GetObjectItem(root, "protocol_type");
    if (cJSON_IsNumber(protocol_type)) {
        g_frame_param_ctx.protocol_type = protocol_type->valueint;
    }

    cJSON *channum = cJSON_GetObjectItem(root, "chan_num");
    if (!cJSON_IsNumber(channum)) {
        cJSON_Delete(root);
        return -1;
    }

    g_frame_param_ctx.channum = channum->valueint;
    if (g_frame_param_ctx.channum > FRAME_MAX_CHAN_NUM) {
        g_frame_param_ctx.channum = FRAME_MAX_CHAN_NUM;
    }

    int index = 0;
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, root) {

        if (!item->string || strcmp(item->string, "chan_num") == 0) {
            continue;
        }

        if (index >= g_frame_param_ctx.channum) {
            break;
        }

        cJSON *chan   = cJSON_GetObjectItem(item, "chan");
        cJSON *width  = cJSON_GetObjectItem(item, "width");
        cJSON *height = cJSON_GetObjectItem(item, "height");

        if (!cJSON_IsNumber(chan) ||
            !cJSON_IsNumber(width) ||
            !cJSON_IsNumber(height)) {
            continue;
        }

        frame_param_item_t *dst = &g_frame_param_ctx.items[index];

        strncpy(dst->topic, item->string, FRAME_TOPIC_MAX_LEN - 1);
        dst->chan   = chan->valueint;
        dst->width  = width->valueint;
        dst->height = height->valueint;

        index++;
    }

    cJSON_Delete(root);
    return 0;
}

int frame_param_get(const char *topic, int *width, int *height)
{
    if (!topic || !width || !height) {
        return -1;
    }

    for (int i = 0; i < g_frame_param_ctx.channum; i++) {
        if (strcmp(g_frame_param_ctx.items[i].topic, topic) == 0) {
            *width  = g_frame_param_ctx.items[i].width;
            *height = g_frame_param_ctx.items[i].height;
            return 0;
        }
    }

    return -1;
}

int frame_param_get_topic_by_chan(int chan, char *topic, int topic_len)
{
    if (!topic || topic_len <= 0) {
        return -1;
    }

    for (int i = 0; i < g_frame_param_ctx.channum; i++) {
        if (g_frame_param_ctx.items[i].chan == chan) {
            strncpy(topic,
                    g_frame_param_ctx.items[i].topic,
                    topic_len - 1);
            topic[topic_len - 1] = '\0';
            return 0;
        }
    }

    return -1;
}

const char *frame_param_get_local_ip(void)
{
    return g_frame_param_ctx.local_ip;
}

int frame_param_get_local_port(void)
{
    return g_frame_param_ctx.local_port;
}

int frame_param_get_protocol_type(void)
{
    return g_frame_param_ctx.protocol_type;
}

