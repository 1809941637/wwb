
#include "veh_param.h"  
#include "common.h"

static veh_control_param_t g_stVehControlParam;
static veh_state_param_t g_stVehSatateParam;

extern void can_gateway_vehicle_control_send(int steering_angle, int angularVelocity, int accelerator_percent, int brake_percent, uint8_t gear);

int veh_control_param_get(veh_control_param_t *pstVehParam)
{
    memcpy(pstVehParam, &g_stVehControlParam, sizeof(veh_control_param_t));
    return 0;
}

int veh_state_param_get(veh_state_param_t *pstVehParam)
{
    memcpy(pstVehParam, &g_stVehSatateParam, sizeof(veh_state_param_t));
    return 0;
}

char veh_printf_debug_get(void) 
{
    return g_stVehControlParam.debug;
}

int veh_printf_debug_set(char value) 
{
    g_stVehControlParam.debug = value > 0 ? 1:0;
    return 0;
}

void veh_state_speed_set(int value)
{
    g_stVehSatateParam.veh_speed = value;
    return;
}

void veh_state_soc_set(char value)
{
    g_stVehSatateParam.soc = value;
    return;
}

void veh_state_fault_grade_set(char value)
{
    g_stVehSatateParam.fault_grade = value;
    return;
}


int veh_param_webrtc_trans_prase(const char *data, int len) 
{
    int data_type =0;

    if(NULL == data) {
        return -1;
    }

    cJSON *json = cJSON_ParseWithLength(data, len);
    if (json == NULL) {
        ERROR("json[%s] parse failed\n", data);
        return -1; 
    }
    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (cJSON_IsNumber(type)) {
        data_type = type->valueint;
    }

    /*1:控制模式 2：控制数据*/
    if(data_type == 1) {
        cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
        if (cJSON_IsNumber(mode)) {
            g_stVehControlParam.veh_mode = mode->valueint;
        }
    }
    else if(data_type == 2) {
        cJSON *direction = cJSON_GetObjectItemCaseSensitive(json, "direction");
        if (cJSON_IsNumber(direction)) {
            g_stVehControlParam.direction = direction->valueint;
        }

        cJSON *throttle = cJSON_GetObjectItemCaseSensitive(json, "throttle");
        if (cJSON_IsNumber(throttle)) {
            g_stVehControlParam.throttle = throttle->valueint;
        }

        cJSON *brake = cJSON_GetObjectItemCaseSensitive(json, "brake");
        if (cJSON_IsNumber(brake)) {
            g_stVehControlParam.brake = brake->valueint;
        }

        cJSON *gearPosition = cJSON_GetObjectItemCaseSensitive(json, "gearPosition");
        if (cJSON_IsString(gearPosition) && gearPosition->valuestring != NULL) {
            if (strcmp(gearPosition->valuestring, "P") == 0) {
                g_stVehControlParam.gear = GEAR_P;
            }  
            else if (strcmp(gearPosition->valuestring, "D") == 0) {
                g_stVehControlParam.gear = GEAR_D;
            }
            else if (strcmp(gearPosition->valuestring, "N") == 0) {
                g_stVehControlParam.gear = GEAR_N;
            }
            else if (strcmp(gearPosition->valuestring, "R") == 0) 
            { 
                g_stVehControlParam.gear = GEAR_R;
            }
            else {
                g_stVehControlParam.gear = GEAR_UNKNOWN;
            }                                              
        }

        cJSON *angularVelocity = cJSON_GetObjectItemCaseSensitive(json, "angularVelocity");
        if (cJSON_IsNumber(angularVelocity)) {
            g_stVehControlParam.angularVelocity = angularVelocity->valueint;
        }

        cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(json, "timestamp");
        if (cJSON_IsNumber(timestamp)) {
            g_stVehControlParam.timestamp = (unsigned long int)timestamp->valuedouble;
        }

        can_gateway_vehicle_control_send(g_stVehControlParam.direction, g_stVehControlParam.angularVelocity ,g_stVehControlParam.throttle, g_stVehControlParam.brake, g_stVehControlParam.gear);    

    }
    else {
        ERROR("unknow type:%d\n", data_type);
    }

    cJSON_Delete(json);

    return 0; 
}

char *veh_param_webrtc_send_create_json_string(void) 
{
     cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "speed", g_stVehSatateParam.veh_speed);
    cJSON_AddNumberToObject(root, "mode", g_stVehControlParam.veh_mode);
    cJSON_AddNumberToObject(root, "soc", g_stVehSatateParam.soc);
    cJSON_AddNumberToObject(root, "faultGrade", g_stVehSatateParam.fault_grade);
    //cJSON_AddStringToObject(root, "message", message);      // 字符串类型

    char *json_string = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);

    return json_string;
}
