#include "webrtc_upload_hook.h"
#include "log_comm.h"
#include "trro_webrtc_interface.h"
#include <fstream>
#include <string>
#include <cstdint>
#include <iostream>
#include "veh_param.h"

namespace vla {

void WebRTCUploadHook::OnVideoFrame(const std::string& topic,
                                   const MosFrameData& frame) {
    if (frame.payload.empty()) {
        ERROR("frame.payload empty\n");
        return;
    }

    if(veh_printf_debug_get()) {
        INFO("[WebRTCUpload] topic=%s size=%zu width=%d height=%d type:%d\n", topic.c_str(), frame.payload.size(), frame.width, frame.height, frame.type);
    }

    if(frame.type) {
        int ret = TrroWebRTCInterface::Instance().SendJpeg(frame.payload.data(), frame.payload.size(), frame.width, frame.height, 0);
        if(1 != ret) {
            ERROR("!!!!SendJpeg fail, ret:%d\n",ret); 
        }
    }
    else {
        int ret = TrroWebRTCInterface::Instance().SendH264(frame.payload.data(), frame.payload.size(), frame.width, frame.height, 1, 0);
        if(1 != ret) {
            ERROR("!!!!SendH264 fail, ret:%d\n",ret); 
        }
    }
    

}

}  // namespace vla
