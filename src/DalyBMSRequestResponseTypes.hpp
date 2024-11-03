// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#pragma once

#include "DalyBMSRequestResponseBase.hpp"

#include <cstdint>
#include <bitset>
#include <array>
#include <vector>

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

namespace daly_bms { 

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

/*
  offical 0x90-0x98
  https://robu.in/wp-content/uploads/2021/10/Daly-CAN-Communications-Protocol-V1.0-1.pdf

  unofficial
  https://diysolarforum.com/threads/decoding-the-daly-smartbms-protocol.21898/
  https://diysolarforum.com/threads/daly-bms-communication-protocol.65439/
*/

template<uint8_t COMMAND>
class RequestResponseCommand : public RequestResponse {
public:
    RequestResponseCommand()
        : RequestResponse(RequestResponse::Builder().setCommand(COMMAND)) {}
protected:
    using RequestResponse::setValid;
    using RequestResponse::setResponseFrameCount;
};

template<uint8_t COMMAND>
bool operator==(const RequestResponseCommand<COMMAND>& lhs, const uint8_t rhs) {
    return rhs == COMMAND;
}
template<uint8_t COMMAND>
bool operator==(const uint8_t rhs, const RequestResponseCommand<COMMAND>& lhs) {
    return rhs == COMMAND;
}

template<uint8_t COMMAND, int LENGTH = 1>
class RequestResponse_TYPE_STRING : public RequestResponseCommand<COMMAND> {
public:
    String string;
    RequestResponse_TYPE_STRING()
        : RequestResponseCommand<COMMAND>() {
        setResponseFrameCount(LENGTH);
    }
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    using RequestResponseCommand<COMMAND>::setResponseFrameCount;
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t frameNum) override {
        if (frameNum == 1) string = "";
        for (size_t i = 0; i < RequestResponseFrame::Constants::SIZE_DATA - 1; i++)
            string += static_cast<char>(frame.getUInt8(1 + i));
        if (frameNum == LENGTH) {
            string.trim();
            return setValid();
        }
        return true;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

struct FrameTypeDateYMD {
    uint8_t year{}, month{}, day{};  // 0-99 + 2000
    String toString() const {
        return String("20") + (year < 10 ? "0" : "") + String(year) + (month < 10 ? "0" : "") + String(month) + (day < 10 ? "0" : "") + String(day);
    }
};
template<typename TYPE>
struct FrameTypeMinmax {
    TYPE min{}, max{};
};
template<typename TYPE>
struct FrameTypeThresholdsMinmax {
    FrameTypeMinmax<TYPE> L1{}, L2{};
};
template <typename TYPE>
struct FrameTypeThresholdsDifference {
    TYPE L1{}, L2{};
};

class FrameContentDecoder {
public:
    static bool decode_Percent_d(const RequestResponseFrame& frame, size_t offset, float* value) {
        *value = static_cast<float>(frame.getUInt16(offset)) / 10.0f;
        return true;
    }
    static bool decode_Voltage_d(const RequestResponseFrame& frame, size_t offset, float* value) {
        *value = static_cast<float>(frame.getUInt16(offset)) / 10.0f;
        return true;
    }
    static bool decode_Voltage_m(const RequestResponseFrame& frame, size_t offset, float* value) {
        *value = static_cast<float>(frame.getUInt16(offset)) / 1000.0f;
        return true;
    }
    static bool decode_Current_d(const RequestResponseFrame& frame, size_t offset, float* value) {
        *value = (static_cast<float>(frame.getUInt16(offset)) - 30000) / 10.0f;
        return true;
    }
    static bool decode_Temperature(const RequestResponseFrame& frame, size_t offset, int8_t* value) {
        *value = frame.getUInt8(offset) - 40;
        return true;
    }
    static bool decode_Time_s(const RequestResponseFrame& frame, size_t offset, uint16_t* value) {
        *value = static_cast<uint16_t>(frame.getUInt8(offset)) * 60;
        return true;
    }
    static bool decode_BitNoFrameNum(const RequestResponseFrame& frame, size_t offset, uint8_t* value) {
        *value = frame.getBit(offset - 1);    // will be offset by 1 due to assumed frame num
        return true;
    }

    template<typename TYPE>
    static typename std::enable_if<std::is_enum<TYPE>::value, bool>::type
    decode(const RequestResponseFrame& frame, size_t offset, TYPE* value) {
        *value = static_cast<TYPE>(frame.getUInt8(offset));
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, std::array<bool, 8>* value) {
        for (size_t i = 0; i < 8; i++)
            (*value)[i] = frame.getBit(offset, i);
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, FrameTypeDateYMD* value) {
        *value = { .year = frame.getUInt8(offset + 0), .month = frame.getUInt8(offset + 1), .day = frame.getUInt8(offset + 2) };
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, bool* value) {
        *value = frame.getUInt8(offset);
        return true;
    }
    template<size_t N>
    static bool decode(const RequestResponseFrame& frame, size_t offset, std::array<uint8_t, N>* values) {
        for (size_t i = 0; i < N; i++)
            (*values)[i] = frame.getUInt8(offset + i);
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, uint8_t* value) {
        *value = frame.getUInt8(offset);
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, uint16_t* value) {
        *value = frame.getUInt16(offset);
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, float* value, float divisor = 1.0f) {
        *value = static_cast<float>(frame.getUInt16(offset)) / divisor;
        return true;
    }
    static bool decode(const RequestResponseFrame& frame, size_t offset, double* value, double divisor = 1.0) {
        *value = static_cast<double>(frame.getUInt32(offset)) / divisor;
        return true;
    }
};

class RequestResponse_BATTERY_RATINGS : public RequestResponseCommand<0x50> {
public:
    double packCapacityAh{};
    double nominalCellVoltage{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &packCapacityAh, 1000.0) && FrameContentDecoder::decode(frame, 4, &nominalCellVoltage, 1000.0));
    }
};

class RequestResponse_BMS_HARDWARE_CONFIG : public RequestResponseCommand<0x51> {
public:
    uint8_t boards{};
    std::array<uint8_t, 3> cells{};
    std::array<uint8_t, 3> sensors{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &boards) && FrameContentDecoder::decode<3>(frame, 1, &cells) && FrameContentDecoder::decode<3>(frame, 4, &sensors));
    }
};

class RequestResponse_BATTERY_STAT : public RequestResponseCommand<0x52> {    // XXX TBC
public:
    double cumulativeChargeAh{}; // XXX should be custom type "Cumulative"
    double cumulativeDischargeAh{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &cumulativeChargeAh) && FrameContentDecoder::decode(frame, 4, &cumulativeDischargeAh));
    }
};

class RequestResponse_BATTERY_INFO : public RequestResponseCommand<0x53> {    // XXX TBC
public:
    uint8_t operationalMode{};
    uint8_t type{};
    FrameTypeDateYMD productionDate{};
    uint16_t automaticSleepSec{};
    uint8_t unknown1{}, unknown2{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &operationalMode) && FrameContentDecoder::decode(frame, 1, &type) && FrameContentDecoder::decode(frame, 2, &productionDate) && FrameContentDecoder::decode_Time_s(frame, 5, &automaticSleepSec) && FrameContentDecoder::decode(frame, 6, &unknown1) && FrameContentDecoder::decode(frame, 7, &unknown2));
    }
};

using RequestResponse_BMS_FIRMWARE_INDEX = RequestResponse_TYPE_STRING<0x54, 1>;

using RequestResponse_BATTERY_CODE = RequestResponse_TYPE_STRING<0x57, 5>;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND, typename TYPE, int SIZE, auto DECODER>
class RequestResponse_TYPE_THRESHOLD_MINMAX : public RequestResponseCommand<COMMAND> {
public:
    FrameTypeThresholdsMinmax<TYPE> value{};
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid( // XXX should be template
            DECODER(frame, SIZE * 0, &value.L1.max) && DECODER(frame, SIZE * 1, &value.L2.max) && DECODER(frame, SIZE * 2, &value.L1.min) && DECODER(frame, SIZE * 3, &value.L2.min));
    }
};

using RequestResponse_CELL_VOLTAGES_THRESHOLDS = RequestResponse_TYPE_THRESHOLD_MINMAX<0x59, float, 2, FrameContentDecoder::decode_Voltage_m>;
using RequestResponse_PACK_VOLTAGES_THRESHOLDS = RequestResponse_TYPE_THRESHOLD_MINMAX<0x5A, float, 2, FrameContentDecoder::decode_Voltage_d>;
using RequestResponse_PACK_CURRENTS_THRESHOLDS = RequestResponse_TYPE_THRESHOLD_MINMAX<0x5B, float, 2, FrameContentDecoder::decode_Current_d>;

class RequestResponse_PACK_TEMPERATURE_THRESHOLDS : public RequestResponseCommand<0x5C> {
public:
    FrameTypeThresholdsMinmax<int8_t> charge{}, discharge{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid( // XXX should be template
            FrameContentDecoder::decode_Temperature(frame, 0, &charge.L1.max) &&
             FrameContentDecoder::decode_Temperature(frame, 1, &charge.L2.max) && 
             FrameContentDecoder::decode_Temperature(frame, 2, &charge.L1.min) && 
             FrameContentDecoder::decode_Temperature(frame, 3, &charge.L2.min) && 
             FrameContentDecoder::decode_Temperature(frame, 4, &discharge.L1.max) && 
             FrameContentDecoder::decode_Temperature(frame, 5, &discharge.L2.max) &&
             FrameContentDecoder::decode_Temperature(frame, 6, &discharge.L1.min) && 
             FrameContentDecoder::decode_Temperature(frame, 7, &discharge.L2.min));
    }
};

using RequestResponse_PACK_SOC_THRESHOLDS = RequestResponse_TYPE_THRESHOLD_MINMAX<0x5D, float, 2, FrameContentDecoder::decode_Percent_d>;

class RequestResponse_CELL_SENSORS_THRESHOLDS : public RequestResponseCommand<0x5E> {
public:
    FrameTypeThresholdsDifference<float> voltage{};
    FrameTypeThresholdsDifference<int8_t> temperature{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(  // XXX should be template
            FrameContentDecoder::decode_Voltage_m(frame, 0, &voltage.L1) && FrameContentDecoder::decode_Voltage_m(frame, 2, &voltage.L2) && 
            FrameContentDecoder::decode_Temperature(frame, 4, &temperature.L1) && FrameContentDecoder::decode_Temperature(frame, 5, &temperature.L2));
    }
};

class RequestResponse_CELL_BALANCES_THRESHOLDS : public RequestResponseCommand<0x5F> {
public:
    float voltageEnableThreshold{};
    float voltageAcceptableDifference{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Voltage_m(frame, 0, &voltageEnableThreshold) && FrameContentDecoder::decode_Voltage_m(frame, 2, &voltageAcceptableDifference));
    }
};

class RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS : public RequestResponseCommand<0x60> {
public:
    float currentShutdownA{};
    float currentSamplingR{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &currentShutdownA) && FrameContentDecoder::decode(frame, 2, &currentSamplingR, 1000.0f));
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponse_BMS_RTC : public RequestResponseCommand<0x61> {    // XXX TBC
public:
    uint32_t dateTime1{}, dateTime2{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        dateTime1 = frame.getUInt32(0);    // XXX ??
        dateTime2 = frame.getUInt32(4);    // XXX ??
        return setValid();
    }
};

using RequestResponse_BMS_SOFTWARE_VERSION = RequestResponse_TYPE_STRING<0x62, 2>;

using RequestResponse_BMS_HARDWARE_VERSION = RequestResponse_TYPE_STRING<0x63, 2>;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponse_PACK_STATUS : public RequestResponseCommand<0x90> {
public:
    float voltage{};
    float current{};
    float charge{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Voltage_d(frame, 0, &voltage) && FrameContentDecoder::decode_Current_d(frame, 4, &current) && FrameContentDecoder::decode_Percent_d(frame, 6, &charge));
    }
};

template<uint8_t COMMAND, typename TYPE, int SIZE, auto DECODER>
class RequestResponse_TYPE_VALUE_MINMAX : public RequestResponseCommand<COMMAND> {
public:
    FrameTypeMinmax<TYPE> value{};
    FrameTypeMinmax<uint8_t> cellNumber{};
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid( // XXX should be template
            DECODER(frame, 0, &value.max) && 
            FrameContentDecoder::decode(frame, SIZE, &cellNumber.max) && 
            DECODER(frame, SIZE + 1, &value.min) && 
            FrameContentDecoder::decode(frame, SIZE + 1 + SIZE, &cellNumber.min));
    }
};

using RequestResponse_CELL_VOLTAGES_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x91, float, 2, FrameContentDecoder::decode_Voltage_m>;

using RequestResponse_CELL_TEMPERATURES_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x92, int8_t, 1, FrameContentDecoder::decode_Temperature>;

enum class ChargeState : uint8_t { Stationary = 0x00,
                                   Charge = 0x01,
                                   Discharge = 0x02 };
String toString(const ChargeState status) {
    switch (status) {
        case ChargeState::Stationary: return "stationary";
        case ChargeState::Charge: return "charge";
        case ChargeState::Discharge: return "discharge";
        default: return "unknown";
    }
}

class RequestResponse_MOSFET_STATUS : public RequestResponseCommand<0x93> {
public:
    ChargeState state{};
    bool mosChargeState{};
    bool mosDischargeState{};
    uint8_t bmsLifeCycle{};
    double residualCapacityAh{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &state) && FrameContentDecoder::decode(frame, 1, &mosChargeState) && FrameContentDecoder::decode(frame, 2, &mosDischargeState) && FrameContentDecoder::decode(frame, 3, &bmsLifeCycle) && FrameContentDecoder::decode(frame, 4, &residualCapacityAh, 1000.0));
    }
};

class RequestResponse_PACK_INFORMATION : public RequestResponseCommand<0x94> {
public:
    uint8_t numberOfCells{};
    uint8_t numberOfSensors{};
    bool chargerStatus{};
    bool loadStatus{};
    std::array<bool, 8> dioStates{};
    uint16_t cycles{};
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &numberOfCells) && FrameContentDecoder::decode(frame, 1, &numberOfSensors) && FrameContentDecoder::decode(frame, 2, &chargerStatus) && FrameContentDecoder::decode(frame, 3, &loadStatus) && FrameContentDecoder::decode(frame, 4, &dioStates) && FrameContentDecoder::decode(frame, 5, &cycles));
    }
};

template<uint8_t COMMAND, typename TYPE, int SIZE, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, auto DECODER>
class RequestResponse_TYPE_ARRAY : public RequestResponseCommand<COMMAND> {
public:
    std::vector<TYPE> values{};
    bool setCount(const size_t count) {
        if (count > 0 && count < ITEMS_MAX) {
            values.resize(count);
            setResponseFrameCount((count / ITEMS_PER_FRAME) + 1);
            return true;
        }
        return false;
    }
    bool isRequestable() const override {
        return !values.empty();
    }
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    using RequestResponseCommand<COMMAND>::setResponseFrameCount;
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t frameNum) override {
        if (frame.getUInt8(0) != frameNum || frame.getUInt8(0) > (values.size() / ITEMS_PER_FRAME)) return false;
        for (size_t i = 0; i < ITEMS_PER_FRAME && ((frameNum * ITEMS_PER_FRAME) + i) < values.size(); i++)
            if (!DECODER(frame, 1 + i * SIZE, &values[(frameNum * ITEMS_PER_FRAME) + i]))
                return setValid (false); // will block remaining frames
        if (frameNum == (values.size() / ITEMS_PER_FRAME) + 1)
            return setValid();
        return true;
    }
};

using RequestResponse_CELL_VOLTAGES = RequestResponse_TYPE_ARRAY<0x95, float, 2, 48, 3, FrameContentDecoder::decode_Voltage_m>;

using RequestResponse_CELL_TEMPERATURES = RequestResponse_TYPE_ARRAY<0x96, int8_t, 1, 16, 7, FrameContentDecoder::decode_Temperature>;

using RequestResponse_CELL_BALANCES = RequestResponse_TYPE_ARRAY<0x97, uint8_t, 1, 48, 48, FrameContentDecoder::decode_BitNoFrameNum>;

class RequestResponse_FAILURE_STATUS : public RequestResponseCommand<0x98> {
    static constexpr size_t NUM_FAILURE_BYTES = 7;
    static constexpr size_t NUM_FAILURE_CODES = NUM_FAILURE_BYTES * 8;
public:
    bool show{};
    std::bitset<NUM_FAILURE_CODES> bits{};
    size_t count{};
    size_t getFailureList(const char** output, const size_t maxFailures) const {
        size_t c = 0;
        for (size_t i = 0; i < NUM_FAILURE_CODES && count < maxFailures; ++i)
            if (bits[i])
                output[c++] = FAILURE_DESCRIPTIONS[i];
        return c;
    }
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        count = 0;
        for (size_t index = 0; index < NUM_FAILURE_CODES; ++index)
            if ((bits[index] = frame.getBit(index)))
                count++;
        show = frame.getUInt8(7) == 0x03;
        return setValid();
    }
private:
    static constexpr std::array<const char*, NUM_FAILURE_CODES> FAILURE_DESCRIPTIONS = {
        "Cell voltage high level 1", "Cell voltage high level 2", "Cell voltage low level 1", "Cell voltage low level 2", "Pack voltage high level 1", "Pack voltage high level 2", "Pack voltage low level 1", "Pack voltage low level 2",                                                                // Byte 0
        "Charge temperature high level 1", "Charge temperature high level 2", "Charge temperature low level 1", "Charge temperature low level 2", "Discharge temperature high level 1", "Discharge temperature high level 2", "Discharge temperature low level 1", "Discharge temperature low level 2",    // Byte 1
        "Charge current high level 1", "Charge current high level 2", "Discharge current high level 1", "Discharge current high level 2", "SOC high level 1", "SOC high level 2", "SOC low level 1", "SOC low level 2",                                                                                    // Byte 2
        "Cell voltage difference high level 1", "Cell voltage difference high level 2", "Cell temperature difference high level 1", "Cell temperature difference high level 2", "Reserved 3-4", "Reserved 3-5", "Reserved 3-6", "Reserved 3-7",                                                            // Byte 3
        "Charge MOSFET temperature high", "Discharge MOSFET temperature high", "Charge MOSFET temperature sensor fail", "Discharge MOSFET temperature sensor fail", "Charge MOSFET adhesion fail", "Discharge MOSFET adhesion fail", "Charge MOSFET breaker fail", "Discharge MOSFET breaker fail",        // Byte 4
        "AFE acquisition module fail", "Voltage sensor fail", "Temperature sensor fail", "EEPROM storage fail", "RTC fail", "Precharge fail", "Vehicle communication fail", "Network communication fail",                                                                                                  // Byte 5
        "Current sensor module fail", "Voltage sensor module fail", "Short circuit protection fail", "Low voltage no charging", "MOS GPS or soft switch MOS off", "Reserved 6-5", "Reserved 6-6", "Reserved 6-7",                                                                                          // Byte 6
    };
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND>
class RequestResponse_TYPE_ONOFF : public RequestResponseCommand<COMMAND> {
public:
    enum class Setting : uint8_t { Off = 0x00,
                                   On = 0x01 };
    RequestResponseFrame prepareRequest(const Setting setting) {
        return RequestResponse::prepareRequest().setUInt8(4, static_cast<uint8_t>(setting)).finalize();
    }
};

using RequestResponse_BMS_RESET = RequestResponseCommand<0x00>;

using RequestResponse_MOSFET_DISCHARGE = RequestResponse_TYPE_ONOFF<0xD9>;

using RequestResponse_MOSFET_CHARGE = RequestResponse_TYPE_ONOFF<0xDA>;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

} // namespace daly_bms

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
