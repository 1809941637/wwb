#include "can_gateway.h"
#include <cstring>
#include <iostream>
#include "mos_comm/interface/subscriber.hpp"
#include "mos_comm/message/message.h"
#include "mos_comm/utils/register.h"
#include "veh_dbc.h"
#include "vcu_dbc.h"
#include "can_gateway.pb.h"
#include "frame_param.h"
#include "log_comm.h"
#include "veh_param.h"
#include "trro_webrtc_interface.h"


static std::shared_ptr<MOS::communication::Publisher> g_CANTx_Pub;
static std::shared_ptr<MOS::message::Message> g_CANTxMsg;

static std::shared_ptr<MOS::communication::Subscriber> g_CANRx_sub;

static bool g_bSendJson = false;

void can_gateway_pub(const std::string& topic)
{
    MOS::communication::ProtocolInfo proto_info;
    proto_info.protocol_type = MOS::communication::ProtocolType::kProtocolNet;
    proto_info.net_info.local_addr.ip = frame_param_get_local_ip();
    proto_info.net_info.local_addr.port = frame_param_get_local_port();
    int domain_id = 0;

    g_CANTx_Pub = MOS::communication::Publisher::New(domain_id, topic, proto_info);
    g_CANTxMsg  = std::make_shared<MOS::message::Message>();

    INFO("CAN_Gateway Pub init, topic=%s\n", topic.c_str());
}

void can_gateway_send(const CanTxBatch& input)
{
    if (!g_CANTx_Pub || !g_CANTxMsg) {
        ERROR("CAN_Gateway Publisher not initialized!\n");
        return;
    }

    can_gateway::CANMsgDataList TxCANMsg_list;

    for (int i = 0; i < input.TxNum; ++i) {
        const auto& TxCanMsg = input.CANMsgTxList[i];
        auto* CANMsg = TxCANMsg_list.add_messages();

        CANMsg->set_f_canfd(TxCanMsg.ProtocolMode);
        CANMsg->set_canchannel(3);  // CAN4 测试写死
        CANMsg->set_canid(TxCanMsg.ID);
        CANMsg->set_candatalen(TxCanMsg.Length);
        CANMsg->set_f_rtr(TxCanMsg.Remote);
        CANMsg->set_f_ide(false);
        CANMsg->set_seconds(0);
        CANMsg->set_nanoseconds(0);
        CANMsg->set_bytes(TxCanMsg.Data, TxCanMsg.Length);
    }

    std::vector<uint8_t> out_bytes;
    out_bytes.resize(TxCANMsg_list.ByteSizeLong());

    if (!TxCANMsg_list.SerializeToArray(out_bytes.data(), out_bytes.size())) {
        ERROR("CAN_Gateway CANMsg Serialize failed\n");
        return;
    }

    auto data_ref = std::make_shared<MOS::message::DataRef>(
        out_bytes.data(), out_bytes.size());

    g_CANTxMsg->SetDataRef(data_ref);
    g_CANTxMsg->SetGenTimestamp(NowNsec());
    g_CANTxMsg->SetVersion(1);

    int ret = g_CANTx_Pub->Pub(g_CANTxMsg);
    if (ret != 0) {
        ERROR("CAN_Gateway Pub failed, ret=%d\n", ret);
    }
}


void can_gateway_veh_control(int steering_angle, int angularVelocity, uint8_t accelerator_percent, uint8_t brake_percent, uint8_t gear)
{
    CanTxBatch ctx;
    ctx.CANMsgTxList.clear();
    struct veh_dbc_ad_control_steering_t steering{};
    struct veh_dbc_ad_control_brake_t brake{};
    struct veh_dbc_ad_control_accelerate_t accelerate{};
    uint8_t data[8] = {0};

    steering_angle = -steering_angle;/*DBC定义左正右负 外设定义左负右正 此处取反*/

    {
        /*转向控制  方向盘转角+角速度*/
        double actual_angle_deg = (double)steering_angle;
        double dbc_angle_deg =  (actual_angle_deg / 450.0) * 30.0;
        if(dbc_angle_deg < -30.0) {
            dbc_angle_deg = -30.0;
        }
        else if(dbc_angle_deg > 30.0) {
            dbc_angle_deg = 30.0;
        }

        double actual_angularVelocity = (double)angularVelocity;
        double dbc_angularVelocity = (actual_angularVelocity / 450.0) * 30.0;
        if(dbc_angularVelocity > 27.86) {
            dbc_angularVelocity = 27.86;
        } 
        else if(dbc_angularVelocity < 2.68) {
            dbc_angularVelocity = 2.68;
        }

        veh_dbc_ad_control_steering_init(&steering);
        steering.ad_steering_valid = 1;
        steering.ad_steering_angle_cmd = veh_dbc_ad_control_steering_ad_steering_angle_cmd_encode(dbc_angle_deg);
        steering.ad_steering_speed_cmd = veh_dbc_ad_control_steering_ad_steering_speed_cmd_encode(dbc_angularVelocity);
    
        memset(data, 0 , sizeof(data));
        veh_dbc_ad_control_steering_pack(data, &steering, sizeof(data));

        CANMsgTx msg{};
        msg.ID = 0x502; 
        msg.Length = 8;
        msg.Remote = false;
        msg.ProtocolMode = false;
        memcpy(msg.Data, data, 8);

        ctx.CANMsgTxList.push_back(msg);
    }

    {
        /*制动控制 刹车开合度*/
        if(brake_percent > 100) {
            brake_percent = 100;
        }

        veh_dbc_ad_control_brake_init(&brake);
        brake.ad_dbs_valid = 1;
        brake.ad_brake_pressure_cmd = brake_percent;
        brake.ad_awsc_flag = 0;
        brake.ad_dbs_workmode = 0;
        
        memset(data, 0 , sizeof(data));
        veh_dbc_ad_control_brake_pack(data, &brake, sizeof(data));

        CANMsgTx msg{};
        msg.ID = 0x503;
        msg.Length = 8;
        msg.Remote = false;
        msg.ProtocolMode = false;
        memcpy(msg.Data, data, 8);

        ctx.CANMsgTxList.push_back(msg);
    }
       
    {
        /*驱动控制 油门开合度+档位*/
        if(accelerator_percent > 100) {
            accelerator_percent = 0;
        }
        if(gear > 3) {
            gear = 0;
        }

        veh_dbc_ad_control_accelerate_init(&accelerate);
        accelerate.ad_accelerate_valid = 1;
        accelerate.ad_energy_recovery = 0;
        accelerate.ad_accelerate_gear = gear;
        accelerate.ad_accelerate_work_mode = 0;/*0：驱动由踏板控制 1：驱动由速度控制 2：扭矩控制*/
        accelerate.ad_torque_control = accelerator_percent;
        accelerate.ad_speedor_torque_control = 0;

        memset(data, 0 , sizeof(data));
        veh_dbc_ad_control_accelerate_pack(data, &accelerate, sizeof(data));

        CANMsgTx msg{};
        msg.ID = 0x504;   
        msg.Length = 8;
        msg.Remote = false;
        msg.ProtocolMode = false;
        memcpy(msg.Data, data, 8);

        ctx.CANMsgTxList.push_back(msg);
    }
    
    {
        memset(data, 0 , sizeof(data));
        data[0] = 0x01;
        data[1] = 0x00;
        data[2] = 0x00;
        data[3] = 0x00;
        data[4] = 0x00;
        data[5] = 0x00;
        data[6] = 0x00;
        data[7] = 0x00;

        CANMsgTx msg{};
        msg.ID = 0x506;   
        msg.Length = 8;
        msg.Remote = false;
        msg.ProtocolMode = false;
        memcpy(msg.Data, data, 8);

        ctx.CANMsgTxList.push_back(msg);
    }

    {
        memset(data, 0 , sizeof(data));
        data[0] = 0x00;
        data[1] = 0x00;
        data[2] = 0x00;
        data[3] = 0x00;
        data[4] = 0x00;
        data[5] = 0x00;
        data[6] = 0x00;
        data[7] = 0x00;
        
        CANMsgTx msg{};
        msg.ID = 0x507;   
        msg.Length = 8;
        msg.Remote = false;
        msg.ProtocolMode = false;
        memcpy(msg.Data, data, 8);

        ctx.CANMsgTxList.push_back(msg);
    }

    {
        memset(data, 0 , sizeof(data));
        data[0] = 0x01;
        data[1] = 0x00;
        data[2] = 0x00;
        data[3] = 0x00;
        data[4] = 0x00;
        data[5] = 0x00;
        data[6] = 0x00;
        data[7] = 0x00;
        
        CANMsgTx msg{};
        msg.ID = 0x508;   
        msg.Length = 8;
        msg.Remote = false;
        msg.ProtocolMode = false;
        memcpy(msg.Data, data, 8);

        ctx.CANMsgTxList.push_back(msg);
    }
    
    {
        memset(data, 0 , sizeof(data));
        data[0] = 0x02;
        data[1] = 0x1e;
        data[2] = 0x00;
        data[3] = 0x00;
        data[4] = 0x00;
        data[5] = 0x00;
        data[6] = 0x00;
        data[7] = 0x00;
        
        CANMsgTx msg{};
        msg.ID = 0x50a;   
        msg.Length = 8;
        msg.Remote = false;
        msg.ProtocolMode = false;
        memcpy(msg.Data, data, 8);

        ctx.CANMsgTxList.push_back(msg);
    }

    {
        memset(data, 0 , sizeof(data));
        data[0] = 0xaa;
        data[1] = 0x02;
        data[2] = 0x00;
        data[3] = 0x00;
        data[4] = 0x00;
        data[5] = 0x00;
        data[6] = 0x00;
        data[7] = 0x00;
        
        CANMsgTx msg{};
        msg.ID = 0x50b;   
        msg.Length = 8;
        msg.Remote = false;
        msg.ProtocolMode = false;
        memcpy(msg.Data, data, 8);

        ctx.CANMsgTxList.push_back(msg);
    }

    ctx.TxNum = ctx.CANMsgTxList.size(); 

    can_gateway_send(ctx);
}

void sub_can_msg_parse(uint32_t can_id, const uint8_t* data, uint8_t data_len)
{
    if(nullptr == data) {
        return;
    }

    double veh_speed = 0.0;
    struct vcu_dbc_vcu_vehicle_diagnosis_t vehDiagnosis{};
    struct vcu_dbc_vcu_vehicle_status_1_t vehStatus1{};
    struct vcu_dbc_vcu_vehicle_status_2_t vehStatus2{};
    struct vcu_dbc_vcu_vehicle_hv_bat_status_t vehHvbatStatus{};

    switch(can_id) {
        case 0x301:
        {
            vcu_dbc_vcu_vehicle_diagnosis_unpack(&vehDiagnosis, data, data_len);
            veh_state_fault_grade_set(vehDiagnosis.vehicle_fault_grade);
            break;
        }
        case 0x303:
        {
            vcu_dbc_vcu_vehicle_status_1_unpack(&vehStatus1, data, data_len);
            break;
        }
        case 0x304:
        {   
            g_bSendJson = true;
            vcu_dbc_vcu_vehicle_status_2_unpack(&vehStatus2, data, data_len);

            veh_speed = vcu_dbc_vcu_vehicle_status_2_vehicle_speed_decode(vehStatus2.vehicle_speed);
            veh_state_speed_set((int)veh_speed);
            break;
        }
        case 0x30A:
        {
            vcu_dbc_vcu_vehicle_hv_bat_status_unpack(&vehHvbatStatus, data, data_len);
            veh_state_soc_set(vehHvbatStatus.vehicle_soc);
            break;
        }
           
        default:
            break;
    }

    return;
}

void MOSCANRxCallBack(MOS::message::spMsg msg)
{
    auto DataRef = msg->GetDataRef();
    auto DataVec = DataRef->GetDataVec();
    auto VecSize = DataRef->GetDataSizeVec();

    can_gateway::CANMsgDataList RxMsgList;
    if (!RxMsgList.ParseFromArray(DataVec[0], VecSize[0])) {
        std::string error_msg = RxMsgList.InitializationErrorString();
        ERROR("MOSCANRxCallBack ParseFromArray PB Stream failed! error:%s\n", error_msg);
        return;
    }

    int msg_count = RxMsgList.messages_size();
    //INFO("Rx Msg Num:%d\n",msg_count);
    
    for(int i = 0; i < msg_count; i++) {
        const auto& msg = RxMsgList.messages(i);

        uint32_t can_id = msg.canid();
        uint8_t data_len = msg.candatalen();
        const auto& bytes = msg.bytes();
       //int can_channel = msg.canchannel();
        //INFO("recv [%d]CAN ID:0x%x, data_len:%d\n",i, can_id, data_len);
        sub_can_msg_parse(can_id, reinterpret_cast<const uint8_t*>(bytes.data()), data_len);
    }

    /*暂定每接收到一次0x304CAN数据向前端发送一次*/
    if(g_bSendJson) {
        TrroWebRTCInterface::Instance().SendJsonData();
        g_bSendJson = false;
    }

    return;
}

void can_gateway_sub(const std::string& topic)
{
    MOS::communication::ProtocolInfo proto_info_Sub;
    proto_info_Sub.protocol_type =MOS::communication::ProtocolType::kProtocolNet;
    proto_info_Sub.net_info.local_addr.ip = frame_param_get_local_ip();
    proto_info_Sub.net_info.local_addr.port = frame_param_get_local_port();;
    int domain_id = 0;

    g_CANRx_sub = MOS::communication::Subscriber::New(domain_id, topic, proto_info_Sub, MOSCANRxCallBack);

    INFO("CAN_Gateway Sub init, topic=%s\n", topic.c_str());
}

extern "C" {
    void can_gateway_vehicle_control_send(int steering_angle, int angularVelocity, uint8_t accelerator_percent, uint8_t brake_percent, uint8_t gear)
    {
        can_gateway_veh_control(steering_angle, angularVelocity, accelerator_percent, brake_percent, gear);
    }
    
}   

    
