#pragma once

#include "upload_hook.h"

namespace vla {

/**
 * @brief WebRTC 上传 Hook
 */
class WebRTCUploadHook : public UploadHook {
public:
    void OnVideoFrame(const std::string& topic,
                      const MosFrameData& frame) override;
};

}  // namespace vla
