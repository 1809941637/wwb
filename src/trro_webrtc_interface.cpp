/**
 * @file trro_webrtc_interface.cpp
 * @brief webrtc通信实现
 * @author wwb
 * @date 2025-12-16
 * @modified 
 **/
#include "trro_webrtc_interface.h"
#include <cstddef>  
#include "trro_field.h"
#include "log_comm.h"
#include "mos_interface.h"
#include "common.h"
#include "frame_param.h"
#include "veh_param.h"


static std::mutex g_init_mtx;
static std::condition_variable g_init_cv;
static std::atomic<bool> g_signal_ready{false};

static void binary_parse(const char* data, int len, int cmd) {

    if(veh_printf_debug_get()) {
        INFO("[WebRtcBinaryParse][cmd:%d][len:%d][%s][%lld]\n",cmd, len, data);
    }

    /*1:可靠数据 0:不可靠*/
    if(cmd) {
        if(0 != veh_param_webrtc_trans_prase(data, len)) {
         ERROR("parse binary fail\n");
        }
    }
    else {
        ERROR("[binary unreliable data][cmd:%d][len:%d][%s]\n",cmd, len, data);
    }
}

static void OnControlDataCallback(void* context,
                                  const char* controller_id,
                                  const char* msg,
                                  int len,
                                  int qos) {
    (void)context;       
    (void)controller_id; 

    if (!msg || len <= 0) return;

    binary_parse(msg, len, qos);
}

static void OnConnectState(void* context, int stream_id, int state) {
    (void)context;
	INFO("!!!!!!Recv stream connect: stream_id: %d, state: %d\n", stream_id, state);

#if 0
    char topic[64] = {0};
    if(0 != frame_param_get_topic_by_chan(stream_id, topic, sizeof(topic))) {
        ERROR("get topic by chan error; chan:%d\n", stream_id);
        return;
    }
#endif
    
    if(kConnected == state) {
        //vla::StartFrameUpload(topic);
        vla::StartFrameUpload(AVM_FRAME_TOPIC);
        
    }
    else if(kDisconnect == state)
    {
        //vla::StopFrameUpload(topic);
        vla::StopFrameUpload(AVM_FRAME_TOPIC);
    }
}

TrroWebRTCInterface& TrroWebRTCInterface::Instance() {
    static TrroWebRTCInterface inst;
    return inst;
}

TrroWebRTCInterface::TrroWebRTCInterface() {

}

TrroWebRTCInterface::~TrroWebRTCInterface() {
    Stop();
}

bool TrroWebRTCInterface::Init(const char* config_path, const char* license_path) {
    std::lock_guard<std::mutex> lk(g_init_mtx);
    if (g_signal_ready.load()) {
        INFO("[TRRo] Already initialized, skip.\n");
        return true;
    }

    /*注册信令状态回调*/
    TRRO_registerSignalStateCallback(nullptr, [](void *context, SignalState state) {
        (void)context;
		if(state == kTrroReady) {
			INFO("TRRO_init >> init success \n");
			/*注册远端设备控制消息回调*/
            TRRO_registerControlDataCallback(nullptr, OnControlDataCallback);

			/*注册视频连接状态回调函数*/
            TRRO_registerOnState(nullptr, OnConnectState);
            
            /*注册视频媒体状态回调*/
            TRRO_registerMediaState(nullptr, [](void* context, int stream_id, int fps, int bps, int rtt, long long lost, long long packets_send, int stun) {
				(void)context;
                INFO("stream %d, fps %d, bps %d, rtt %d, lost %lld, packets_send %lld, stun %d\n", stream_id, fps, bps, rtt, lost, packets_send, stun);
			});

            TRRO_registerOnErrorEvent(nullptr, [](void* context, int error_code, const char* error_msg) {
				(void)context;
                INFO("error_code %d, error_msg %s\n", error_code, error_msg);
			});

#if 0
            //TRRO_onLatencyReport  --- 远程控车时判断延长，如果太大vcct > 500ms则考虑停车控车，可以做个计数逻辑

			TRRO_registerVideoCaptureCallback(nullptr, [](void *context, const char* data, int width, int height, int type, int stream_id) {});
			
            TRRO_registerLatencyCallback(nullptr, [](void *context, int stream_id, int vcct){
				INFO("latency stream id %d, vcct %d\n", stream_id, vcct);
			});

			TRRO_registerAudioMediaState(nullptr, [](void* context, int stream_id, int fps, int bps, int rtt, long long lost, long long packets_send, int stun) {
				INFO("audio stream %d, fps %d, bps %d, rtt %d, lost %lld, packets_send %lld, stun %d\n", stream_id, fps, bps, rtt, lost, packets_send, stun);
			});

            /*请求master权限时会有这个回调*/
			TRRO_registerOperationPermissionRequest(nullptr, [](void* context, const char* remote_devid, int permission) {
				INFO("remote devid %s permission %d\n", remote_devid, permission);
			});

			TRRO_registerMultiNetworkStatsCallback(nullptr, [](void* context, const TrroMultiNetworkStats stats) {
				INFO("multi network local:%s:%d extern:%s:%d rtt %f lost %f recv %ld send %ld\n", 
					stats.local_ip, stats.local_port, stats.extern_ip, stats.extern_port, stats.rtt, stats.lost, stats.recv_bytes, stats.send_bytes);
			});

			TRRO_registerRemoteMixAudioFrameCallback(nullptr, [](void* context, const char* data, int length, int channels, int sample_rate) {
				INFO("remote mix audio frame length %d, sample_rate %d, channels %d\n", length, sample_rate, channels);
			});
#endif
			g_signal_ready = true;
            g_init_cv.notify_one();
		} else if(state == kTrroAuthFailed) {
			ERROR("device_id or password is incorrect\n");
		} else if(state == kTrroKickout) {
			ERROR("mqtt kickout, stop sdk\n");//多设备同时登录时返回，需配置强制登录
		} else if(state == kTrroLost) {
            //g_signal_ready = false;
			ERROR("disconnected , connecting...  \n");
		} else if(state == kTrroReup) {
            //g_signal_ready = true;
			INFO("reconnect success\n");
		}
	});

    int ret = TRRO_initGwPathWithLicense(config_path, license_path, -1);
    if (ret != TRRO_SUCCEED) {
        ERROR("TRRO_init failed: %s\n", getErrorMsg(ret));
        return false;
    }

    return true;
}

void TrroWebRTCInterface::Start() {
    std::unique_lock<std::mutex> lk(g_init_mtx);
    g_init_cv.wait(lk, [] { return g_signal_ready.load(); });

    int retry = 0;
    const int MAX_RETRY = 3;
    while (retry < MAX_RETRY) {
        int ret = TRRO_start();
        if (ret == TRRO_SUCCEED) {
            INFO("[TRRo] Started successfully\n");
            return;
        } else if (ret == -TRRO_INIT_PUBLIC_LICENSE_CHECK_TIMEOUT) {
            retry++;
            ERROR("[TRRo] License check timeout, retry %d/%d\n", retry, MAX_RETRY);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } else {
            ERROR("TRRO_start failed: %s\n", getErrorMsg(ret));
            break;
        }
    }
    INFO("[TRRo] Start failed after max retries\n");
}

void TrroWebRTCInterface::Stop() {
    TRRO_stop();
    g_signal_ready = false;
    INFO("[TRRo] Stopped\n");
}

void TrroWebRTCInterface::SendJsonData() {
        
    if(g_signal_ready.load()) {  
        char *data = veh_param_webrtc_send_create_json_string();
        if(nullptr == data) {
            return;
        }

        int data_size = strlen(data);

        bool result = SendBinary(data, data_size, 1);
        if (!result) {
            ERROR("Failed to send binary data\n");
        }
        
        if(veh_printf_debug_get()) {
            INFO("[WebRtcBinarySend][len:%d][%s]\n",data_size, data);
        }
        
        free(data);
    }
}

const char *TrroWebRTCInterface::GetTrroSdkVer(void)
{
    return TRRO_getSdkVersion();
}

bool TrroWebRTCInterface::SendH264(const uint8_t* data, int size, int width, int height, bool is_key_frame, int stream_id) {
    if (!data || size <= 0 || !g_signal_ready.load()) return false;

    FrameType type = is_key_frame ? TYPE_IFrame : TYPE_PFrame;
    int ret = TRRO_externalEncodeVideoData(stream_id, reinterpret_cast<const char*>(data),
                                           width, height, size, type);
    return ret == TRRO_SUCCEED;
}

bool TrroWebRTCInterface::SendJpeg(const uint8_t* data, int size, int width, int height, int stream_id) {
    if (!data || size <= 0 || !g_signal_ready.load()) return false;

    int type = Trro_ColorJPEG;/*Trro_ColorYUVI420 Trro_ColorJPEG Trro_ColorYUYV*/
    //int ret = TRRO_externalVideoData(stream_id, reinterpret_cast<const char*>(data), width, height, type, size, "", 0, 0);
    int ret = TRRO_externalVideoDataWithText(stream_id, reinterpret_cast<const char*>(data), width, height, type, size, nullptr);
    return ret == TRRO_SUCCEED;
}

bool TrroWebRTCInterface::SendBinary(const char* data, int size, int qos) {
    if (!data || size <= 0 || !g_signal_ready.load()) return false;

    int ret = TRRO_sendControlData(reinterpret_cast<const char*>(data), size, qos);
    return ret == TRRO_SUCCEED;
}

extern "C" {
    
    int trro_webrtc_send_binary(const char* data, int size, int qos)
    {
        return TrroWebRTCInterface::Instance().SendBinary(data, size, qos) ? 1 : 0;
    }

}   
