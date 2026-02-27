#pragma once

#include <string>
#include <memory>
#include "mos_interface.h"

namespace vla {

/**
 * @brief 上传回调接口（策略接口）
 */
class UploadHook {
public:
    virtual ~UploadHook() = default;

    /**
     * @param topic 摄像头 topic
     * @param frame H264 码流帧
     */
    virtual void OnVideoFrame(const std::string& topic,
                              const MosFrameData& frame) = 0;
};

using UploadHookPtr = std::shared_ptr<UploadHook>;

}  // namespace vla
