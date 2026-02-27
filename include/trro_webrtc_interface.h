#ifndef TRRO_WEBRTC_INTERFACE_H
#define TRRO_WEBRTC_INTERFACE_H

#include <string>
#include <cstdint>

class TrroWebRTCInterface {
public:

    static TrroWebRTCInterface& Instance();

    TrroWebRTCInterface(const TrroWebRTCInterface&) = delete;
    TrroWebRTCInterface& operator=(const TrroWebRTCInterface&) = delete;
    TrroWebRTCInterface(TrroWebRTCInterface&&) = delete;
    TrroWebRTCInterface& operator=(TrroWebRTCInterface&&) = delete;

    bool Init(const char* config_path, const char* license_path);

    void Start();

    void SendJsonData();

    void Stop();

    const char *GetTrroSdkVer(void);

    void TestNetworkQuality();

    bool SendH264(const uint8_t* data, int size, int width, int height, bool is_key_frame, int stream_id = 0);

    bool SendJpeg(const uint8_t* data, int size, int width, int height, int stream_id);

    bool SendBinary(const char* data, int size, int qos = 0); // qos: 0 unreliable, 1 reliable

private:
    TrroWebRTCInterface();
    ~TrroWebRTCInterface();
};

#endif // TRRO_WEBRTC_INTERFACE_H
