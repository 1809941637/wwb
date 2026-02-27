/**
 * @file mos_interface.cpp
 * @brief 订阅视频MOS中间件实现
 * @author wwb
 * @date 2025-12-16
 * @modified 
 **/

#include "mos_interface.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <iomanip>  
#include <mutex>
#include <thread>
#include "log_comm.h"
#include "mos_comm/interface/subscriber.hpp"
#include "mos_comm/message/message.h"
#include "mos_comm/utils/register.h"
#include "upload_hook.h"
#include "frame_param.h"
#include "common.h"

namespace vla {

namespace {
constexpr int kStopPollIntervalSec = 1;
bool g_comm_inited = false;
}

struct Subscription {
    std::atomic<bool> stop{false};
    std::atomic<bool> enabled{false};
    std::thread thread;
};

class MosInterface {
public:
    bool MosSubTopic(const std::string& topic);

    bool StartFrameUpload(const std::string& topic);
    void StopFrameUpload(const std::string& topic);
    void StopAllFrameUploads();

    void SetUploadHook(const UploadHookPtr& hook) {
        std::lock_guard<std::mutex> lk(mutex_);
        upload_hook_ = hook;
    }

    void ProcessFrame(const std::string& topic, MOS::message::spMsg msg);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Subscription>> subs_;
    UploadHookPtr upload_hook_;
};

bool InitMosCommunication(const std::string& json_file) {
    if (g_comm_inited) {
        WARN("MOS communication already initialized\n");
        return true;
    }
    MOS::communication::Init(json_file);
    MOS::utils::Register::get().register_version("example_comm", "1.1.0");
    g_comm_inited = true;
    INFO("MOS communication init ok\n");
    return true;
}

static int config_sub(int domain_id,
                      bool type,
                      const std::string& topic,
                      const std::string& ip,
                      uint32_t port,
                      std::atomic<bool>& stop_flag,
                      MosInterface* interface_ptr) {

    if(topic.empty() || ip.empty() || port == 0) {
        ERROR("MOS START SUB param is error\n");
        return -1;
    }
    INFO("==<<<MOS START SUB topic:%s type:%d local_ip:%s local_port:%d>>>==\n", topic.c_str(), type, ip.c_str(), port);

    MOS::communication::ProtocolInfo proto_info;
    if (type) {
        proto_info.protocol_type = MOS::communication::kProtocolNet;
        proto_info.net_info.local_addr.ip = ip.c_str();
        proto_info.net_info.local_addr.port = port;
    } else {
        proto_info.protocol_type = MOS::communication::kProtocolShm;
    }

    auto subscriber = MOS::communication::Subscriber::New(domain_id, topic, proto_info,
        [interface_ptr, topic](MOS::message::spMsg msg) {
            if (interface_ptr) {
                interface_ptr->ProcessFrame(topic, msg);
            }
        });

    while (!stop_flag.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(kStopPollIntervalSec));
    }

    INFO("==<<<MOS END SUB %s>>>==\n", topic.c_str());
    return 0;
}

void MosInterface::ProcessFrame(const std::string& topic, MOS::message::spMsg msg) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = subs_.find(topic);
        if (it == subs_.end() || !it->second->enabled.load() || !upload_hook_) {
            return;
        }
    }

    if (!msg) {
        ERROR("ptr is null\n");
        return;
    }

    MosFrameData frame;
    frame.sequence_id = msg->GetSeqId();
    frame.timestamp_ns = msg->GetGenTimestamp();
    frame.writer_id = msg->GetWriterId();

    int width, height; 
    if(0 != frame_param_get(topic.c_str(), &width, &height)) {
        ERROR("get frame width and height by topic error! topic:%s\n", topic.c_str());
        return;
    }
    frame.width = width;
    frame.height = height;
    if(0 == strcmp(topic.c_str(), AVM_FRAME_TOPIC)) {
        frame.type = 1;/*判断是H264编码还是七合一的JPEG编码数据*/
    }

    // ext data
    char* ext = nullptr;
    auto ext_size = msg->GetExtData(const_cast<const char**>(&ext));
    if (ext_size > 0 && ext) {
        frame.ext_metadata.assign(ext, ext_size);
    }

    auto data_vec = msg->GetDataRef()->GetDataVec();
    auto data_size_vec = msg->GetDataRef()->GetDataSizeVec();

#if 0
    auto size = data_size_vec.size();
    for (std::size_t i = 0; i < size; i++) {
        auto vec_size = data_size_vec[i];
        uint8_t* vec_data = static_cast<uint8_t*>(data_vec[i]);
        std::cout << "Receive " << i << " msg:" << std::endl;
        for (int j = 0; j < vec_size; j++) {
            if ((j + 1) % 10 == 0) {
                std::cout << std::endl;
            }
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<char>(vec_data[j]) << " ";
        }
        std::cout << std::endl << std::endl;
    }
#else
    size_t total = 0;
    for (auto s : data_size_vec) {
        total += s;
    }
    frame.payload.resize(total);
    size_t offset = 0;
    for (size_t i = 0; i < data_vec.size(); ++i) {
        std::memcpy(frame.payload.data() + offset,
                    data_vec[i],
                    data_size_vec[i]);
        offset += data_size_vec[i];
    }

    upload_hook_->OnVideoFrame(topic, frame);
#endif
    
}

bool MosInterface::MosSubTopic(const std::string& topic) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!g_comm_inited) {
        ERROR("MOS not initialized, call InitMosCommunication first\n");
        return false;
    }
    if (subs_.count(topic)) {
        WARN("Topic already running: %s\n", topic.c_str());
        return true;
    }

    auto sub = std::make_shared<Subscription>();
    sub->enabled.store(false);

    int domain_id = 0;
    bool type = frame_param_get_protocol_type();
    std::string ip = frame_param_get_local_ip();
    uint32_t port = frame_param_get_local_port();

    sub->thread = std::thread([this, topic, domain_id, type, ip, port, sub]() mutable {
        std::string thread_name = "sub_" + topic.substr(4);
        pthread_setname_np(pthread_self(), thread_name.c_str());
        
        config_sub(domain_id, type, topic, ip, port, sub->stop, this);
    });

    subs_[topic] = sub;
    return true;
}

bool MosInterface::StartFrameUpload(const std::string& topic) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = subs_.find(topic);
    if (it == subs_.end()) {
        WARN("No persistent subscription for topic: %s, call MosSubTopic first\n", topic.c_str());
        return false;
    }
    it->second->enabled.store(true);
    INFO("MOS frame upload ENABLED! topic=%s\n", topic.c_str());
    return true;
}

void MosInterface::StopFrameUpload(const std::string& topic) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = subs_.find(topic);
    if (it == subs_.end()) return;
    it->second->enabled.store(false);
    INFO("MOS frame upload DISABLED! topic=%s\n", topic.c_str());
}

void MosInterface::StopAllFrameUploads() {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& kv : subs_) {
        kv.second->enabled.store(false);
        INFO("MOS frame upload DISABLED! topic=%s\n", kv.first.c_str());
    }
}

static MosInterface& Instance() {
    static MosInterface inst;
    return inst;
}

void SetMosUploadHook(const UploadHookPtr& hook) {
    Instance().SetUploadHook(hook);
}

bool MosSubTopic(const std::string& topic) {
    return Instance().MosSubTopic(topic);
}

bool StartFrameUpload(const std::string& topic) {
    return Instance().StartFrameUpload(topic);
}

void StopFrameUpload(const std::string& topic) {
    Instance().StopFrameUpload(topic);
}

void StopAllFrameUploads() {
    Instance().StopAllFrameUploads();
}

} // namespace vla
