#include <stdio.h>
#include "app_sys_cmd.h"
#include <stdlib.h>
#include "common.h"
#include "frame_param.h"
#include "veh_param.h"

extern int trro_webrtc_send_binary(const char* data, int size, int qos);
extern void topi_test(char *topic);

extern void can_gateway_vehicle_control_send(int steering_angle, int angularVelocity, int accelerator_percent, int brake_percent, uint8_t gear);

void help_cmd_app_test(void)
{
    printf("用法: app_test \n");
    printf("参数: \n");
    printf("提示: \n");
    printf("举例: app_test \n");
}

int cmd_app_test(int argc, char *argv[])
{
    if (argc < 2)
    {
        help_cmd_app_test();
        return -1;
    }
    //printf("This is test cmd\n");

    int cmd = atoi(argv[1]);
    char data[64] = {0};
    int width, height;
    char topic[64] = {0};
    int value =0;

    if(argc > 2) {
        value = atoi(argv[2]);
    }

    switch(cmd)
    {
        case 1: 
            printf("this is test!\n");
            break;
        case 2:
            snprintf(data, sizeof(data), "{\"speed\": %d}", value);
            trro_webrtc_send_binary(data, sizeof(data), 1);
            printf("webrtc send test success:[%s]\n", data);
            break;
        case 3:
            frame_param_get(F_FRAME_TOPIC, &width, &height);
            printf("!!!!!!!%s-- width=%d, height=%d\n", F_FRAME_TOPIC, width, height);

            frame_param_get(FN_FRAME_TOPIC, &width, &height);
            printf("!!!!!!!%s-- width=%d, height=%d\n", FN_FRAME_TOPIC, width, height);

            frame_param_get(B_FRAME_TOPIC, &width, &height);
            printf("!!!!!!!%s-- width=%d, height=%d\n", B_FRAME_TOPIC, width, height);

            frame_param_get(LF_FRAME_TOPIC, &width, &height);
            printf("!!!!!!!%s-- width=%d, height=%d\n", LF_FRAME_TOPIC, width, height);

            frame_param_get(RF_FRAME_TOPIC, &width, &height);
            printf("!!!!!!!%s-- width=%d, height=%d\n", RF_FRAME_TOPIC, width, height);

            frame_param_get(LB_FRAME_TOPIC, &width, &height);
            printf("!!!!!!!%s-- width=%d, height=%d\n", LB_FRAME_TOPIC, width, height);

            frame_param_get(RB_FRAME_TOPIC, &width, &height);
            printf("!!!!!!!%s-- width=%d, height=%d\n", RB_FRAME_TOPIC, width, height);

            frame_param_get_topic_by_chan(2,topic,sizeof(topic));
            printf("chan: 2 --- topic: %s\n", topic);

            printf("frame param local_ip:%s, port:%d, protocol_type:%d\n", frame_param_get_local_ip(), frame_param_get_local_port(), frame_param_get_protocol_type());
            break;
        case 4:
            can_gateway_vehicle_control_send(150,3,20,30,GEAR_P);   
            break;


        default:
            break;
    }

    return 0;
}

void help_cmd_app_veh_param(void)
{
    printf("用法: app_veh_param 1\n");
    printf("参数: \n");
    printf("提示: \n");
    printf("举例: app_veh_param 1\n");
}

int cmd_app_veh_param(int argc, char *argv[])
{
    if (argc < 2)
    {
        help_cmd_app_veh_param();
        return -1;
    }
    int value = 0;
    veh_control_param_t stVehParam = {0};
    veh_state_param_t stVehState = {0};

    int cmd = atoi(argv[1]);

    if(argc > 2) {
        value = atoi(argv[2]);
    }

    switch(cmd) 
    {
        case 1:
            veh_control_param_get(&stVehParam);
            veh_state_param_get(&stVehState);
            
            printf("veh_speed:%d\n",stVehState.veh_speed);
            printf("soc:%d\n",stVehState.soc);
            printf("fault_grade:%d\n",stVehState.fault_grade);

            printf("debug:%d\n",stVehParam.debug);
            printf("veh_mode:%d (1:AUTO 2:MANUAL)\n", stVehParam.veh_mode);
            printf("direction:%d\n", stVehParam.direction);
            printf("throttle:%d\n", stVehParam.throttle);
            printf("brake:%d\n", stVehParam.brake);
            printf("gear:%d(0:P 1:D 2:N 3:R)\n", stVehParam.gear);
            printf("angularVelocity:%d\n", stVehParam.angularVelocity);
            printf("timestamp:%lu\n", stVehParam.timestamp);
            break;
        case 2:
            veh_printf_debug_set(value);
            break;

        default:
            break;
    }

    

    return 0;
}
