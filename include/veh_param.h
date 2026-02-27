#ifndef VEH_PARAM_H
#define VEH_PARAM_H


typedef enum {
    GEAR_P = 0,    // 驻车档
    GEAR_D = 1,    // 前进档
    GEAR_N = 2,    // 空档
    GEAR_R = 3,    // 倒车档
    GEAR_UNKNOWN = -1
}gear_position_e;

typedef struct {
    
    char debug;
    int veh_mode;             /*1：自动 2：手动*/
    int direction;            /*方向盘角度*/
    unsigned char throttle;  /*油门开合度*/
    unsigned char brake;     /*刹车开合度*/
    gear_position_e gear;    /*档位*/
    int angularVelocity;    /*方向盘角速度*/
    unsigned long int timestamp; /*时间戳*/
}veh_control_param_t;

typedef struct {
    int veh_speed; /*当前车速 负数为倒车*/
    char soc;  /*剩余电量0~100%*/
    char fault_grade; /*整车故障等级 0:无故障 1:一级故障，不影响车辆行驶 2:二级故障，速度环限制车速最高5km/h，扭矩环不做限制 3:三级故障，最高等级故障 车辆禁止行驶 */

}veh_state_param_t;

typedef enum {

    VEH_MOD_AUTO = 1,
    VEH_MODE_MANUAL = 2,
    
}veh_mod_e;


#ifdef __cplusplus
extern "C" {
#endif

int veh_param_webrtc_trans_prase(const char *data, int len);

char *veh_param_webrtc_send_create_json_string(void);

int veh_control_param_get(veh_control_param_t *pstVehParam);

int veh_state_param_get(veh_state_param_t *pstVehParam);

char veh_printf_debug_get(void);

int veh_printf_debug_set(char value);

void veh_state_speed_set(int value);

void veh_state_soc_set(char value);

void veh_state_fault_grade_set(char value);

#ifdef __cplusplus
}
#endif

#endif 
