#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vla {

/**
 * @brief MOS 视频帧
 */
struct MosFrameData {
    uint64_t sequence_id = 0;
    int64_t timestamp_ns = 0;
    uint64_t writer_id = 0;
    std::string ext_metadata;
    std::vector<uint8_t> payload; // H264
    int32_t width = 0;
    int32_t height = 0;
    int32_t type = 0;/*0-H264 1-Jpeg*/
    
};

class UploadHook;
using UploadHookPtr = std::shared_ptr<UploadHook>;


bool InitMosCommunication(const std::string& json_file);

void SetMosUploadHook(const UploadHookPtr& hook);

bool MosSubTopic(const std::string& topic);

bool StartFrameUpload(const std::string& topic);

void StopFrameUpload(const std::string& topic);

void StopAllFrameUploads();

} // namespace vla
