#include <iostream>
#include <cinttypes>
#include <csignal>
#include "common.h"
#include "log_comm.h"
#include "mos_interface.h"
#include "webrtc_upload_hook.h"
#include "trro_webrtc_interface.h"
#include "app_cmd.h"
#include "frame_param.h"
#include "can_gateway.h"


int main() {

    log_init();

    cmd_server_init();

    frame_param_init(FRAME_PARAM_CONFIG_PATH);

    TrroWebRTCInterface::Instance().Init(TRRO_WEBRTC_CONFIG_PATH, TRRO_WEBRTC_LICENSE_PATH);
    TrroWebRTCInterface::Instance().Start();

    const char* Ver = TrroWebRTCInterface::Instance().GetTrroSdkVer();
    if(nullptr != Ver) {
        INFO("TRRO SDK Ver:%s\n", Ver);
    }

    vla::SetMosUploadHook(std::make_shared<vla::WebRTCUploadHook>());
    vla::InitMosCommunication(MOS_COMM_CONFIG_PATH);

    //vla::MosSubTopic(F_FRAME_TOPIC);
    //vla::MosSubTopic(FN_FRAME_TOPIC);
    //vla::MosSubTopic(B_FRAME_TOPIC);
    //vla::MosSubTopic(LF_FRAME_TOPIC);
    //vla::MosSubTopic(RF_FRA ME_TOPIC);
    //vla::MosSubTopic(LB_FRAME_TOPIC);
    //vla::MosSubTopic(RB_FRAME_TOPIC); 
    vla::MosSubTopic(AVM_FRAME_TOPIC);

    can_gateway_pub(CAN_GATEWAY_PUB_TOPIC);
    can_gateway_sub(CAN_GATEWAY_SUB_TOPIC);


    while (true) {
        sleep(1);
    }

    return 0;
}



