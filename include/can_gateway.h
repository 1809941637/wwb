#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>


struct CANMsgTx {
    uint32_t ID;
    uint8_t  Length;
    bool     Remote;
    bool     ProtocolMode;
    uint8_t  Data[8];
};

struct CanTxBatch {
    std::vector<CANMsgTx> CANMsgTxList;
    int TxNum;
};

void can_gateway_pub(const std::string& topic);

void can_gateway_sub(const std::string& topic);

void can_gateway_send(const CanTxBatch& input);

void can_gateway_veh_control(int steering_angle, int angularVelocity, uint8_t accelerator_percent, uint8_t brake_percent, uint8_t gear);
