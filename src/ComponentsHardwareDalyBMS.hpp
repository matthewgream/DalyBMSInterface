
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "HardwareSerial.h"
#include "ArduinoJson.h"

#include <cstdint>
#include <array>
#include <map>
#include <bitset>

namespace daly_bms {

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponseFrame_Receiver;

class RequestResponseFrame {
public:
    using Receiver = RequestResponseFrame_Receiver;

    struct Constants {
        static constexpr size_t SIZE_FRAME = 13;
        static constexpr size_t SIZE_HEADER = 4;
        static constexpr size_t SIZE_DATA = (SIZE_FRAME - SIZE_HEADER - 1);

        static constexpr size_t OFFSET_BYTE_START = 0;
        static constexpr size_t OFFSET_ADDRESS = 1;
        static constexpr size_t OFFSET_COMMAND = 2;
        static constexpr size_t OFFSET_SIZE = 3;
        static constexpr size_t OFFSET_CHECKSUM = (SIZE_FRAME - 1);

        static constexpr uint8_t VALUE_BYTE_START = 0xA5;
        static constexpr uint8_t VALUE_ADDRESS_HOST = 0x40;
        static constexpr uint8_t VALUE_ADDRESS_SLAVE = 0x01;
    };

    RequestResponseFrame()
        : _data{} {
        setStartByte(Constants::VALUE_BYTE_START);
        setFrameSize(Constants::SIZE_DATA);
    }

    void setAddress(const uint8_t value) {
        _data[Constants::OFFSET_ADDRESS] = value;
    }
    uint8_t getCommand() const {
        return _data[Constants::OFFSET_COMMAND];
    }
    void setCommand(const uint8_t value) {
        _data[Constants::OFFSET_COMMAND] = value;
    }

    const RequestResponseFrame& finalize() {
        _data[Constants::OFFSET_CHECKSUM] = calculateChecksum();
        return *this;
    }

    bool valid() const {
        return _data[Constants::OFFSET_BYTE_START] == Constants::VALUE_BYTE_START && _data[Constants::OFFSET_ADDRESS] != Constants::VALUE_ADDRESS_SLAVE && _data[Constants::OFFSET_SIZE] == Constants::SIZE_DATA && _data[Constants::OFFSET_CHECKSUM] == calculateChecksum();
    }

    //

    inline uint8_t getUInt8(const size_t offset) const {
        validateDataOffset(offset);
        return _data[Constants::SIZE_HEADER + offset];
    }
    inline RequestResponseFrame& setUInt8(const size_t offset, const uint8_t value) {
        validateDataOffset(offset);
        _data[Constants::SIZE_HEADER + offset] = value;
        return *this;
    }
    inline uint16_t getUInt16(const size_t offset) const {
        validateDataOffset(offset);
        return ((uint16_t)_data[Constants::SIZE_HEADER + offset] << 8) | (uint16_t)_data[Constants::SIZE_HEADER + offset + 1];
    }
    inline uint32_t getUInt32(const size_t offset) const {
        validateDataOffset(offset);
        return ((uint32_t)_data[Constants::SIZE_HEADER + offset] << 24) | ((uint32_t)_data[Constants::SIZE_HEADER + offset + 1] << 16) | ((uint32_t)_data[Constants::SIZE_HEADER + offset + 2] << 8) | (uint32_t)_data[Constants::SIZE_HEADER + offset + 3];
    }
    inline bool getBit(const size_t offset, const uint8_t position) const {
        validateDataOffset(offset);
        return (_data[Constants::SIZE_HEADER + offset] >> position) & 0x01;
    }
    inline bool getBit(const size_t index) const {
        validateDataOffset(index >> 3);
        return (_data[Constants::SIZE_HEADER + (index >> 3)] >> (index & 0x07)) & 0x01;
    }

    const uint8_t* data() const {
        return _data.data();
    }
    static constexpr size_t size() {
        return Constants::SIZE_FRAME;
    }

    String toString() const {
        String result;
        result.reserve(Constants::SIZE_FRAME * 3);
        static const char HEX_CHARS[] = "0123456789ABCDEF";
        for (size_t i = 0; i < Constants::SIZE_FRAME; i++) {
            if (i > 0) result += ' ';
            result += HEX_CHARS[_data[i] >> 4];
            result += HEX_CHARS[_data[i] & 0x0F];
        }
        return result;
    }

protected:
    friend Receiver;
    uint8_t& operator[](const size_t index) {
        assert(index < Constants::SIZE_FRAME);
        return _data[index];
    }
    const uint8_t& operator[](const size_t index) const {
        assert(index < Constants::SIZE_FRAME);
        return _data[index];
    }

private:
    void setStartByte(const uint8_t value) {
        _data[Constants::OFFSET_BYTE_START] = value;
    }
    void setFrameSize(const uint8_t value) {
        _data[Constants::OFFSET_SIZE] = value;
    }

    inline void validateDataOffset(const size_t offset) const {
        assert(offset < Constants::SIZE_DATA);
    }

    uint8_t calculateChecksum() const {
        uint8_t sum = 0;
        for (int i = 0; i < Constants::OFFSET_CHECKSUM; i++)
            sum += _data[i];
        return sum;
    }

    std::array<uint8_t, Constants::SIZE_FRAME> _data;
};

// -----------------------------------------------------------------------------------------------

class RequestResponseFrame_Receiver {

protected:
    virtual bool getByte(uint8_t* byte) = 0;
    virtual bool sendBytes(const uint8_t* data, const size_t size) = 0;

public:
    using Handler = std::function<void(const RequestResponseFrame&)>;

    enum class ReadState {
        WaitingForStart,
        ReadingHeader,
        ReadingData
    };

    RequestResponseFrame_Receiver()
        : _handler([](const RequestResponseFrame&) {}), _listener([](const RequestResponseFrame&) {}) {}
    virtual ~RequestResponseFrame_Receiver() = default;

    void registerHandler(Handler handler) {
        _handler = std::move(handler);
    }
    void registerListener(Handler listener) {
        _listener = std::move(listener);
    }

    virtual void begin() {}
    void process() {
        uint8_t byte;
        while (getByte(&byte)) {
            switch (_readState) {
                case ReadState::WaitingForStart:
                    if (byte == RequestResponseFrame::Constants::VALUE_BYTE_START) {
                        _readFrame[RequestResponseFrame::Constants::OFFSET_BYTE_START] = byte;
                        _readOffset = RequestResponseFrame::Constants::OFFSET_ADDRESS;
                        _readState = ReadState::ReadingHeader;
                    }
                    break;

                case ReadState::ReadingHeader:
                    _readFrame[_readOffset++] = byte;
                    if (_readOffset == RequestResponseFrame::Constants::SIZE_HEADER) {
                        if (_readFrame[RequestResponseFrame::Constants::OFFSET_ADDRESS] >= RequestResponseFrame::Constants::VALUE_ADDRESS_SLAVE) {    // SLEEPING
                            _readState = ReadState::WaitingForStart;
                            _readOffset = RequestResponseFrame::Constants::OFFSET_BYTE_START;
                        } else {
                            _readState = ReadState::ReadingData;
                        }
                    }
                    break;

                case ReadState::ReadingData:
                    _readFrame[_readOffset++] = byte;
                    if (_readOffset == RequestResponseFrame::Constants::SIZE_FRAME) {
                        if (_readFrame.valid())
                            _handler(_readFrame);
                        _readState = ReadState::WaitingForStart;
                        _readOffset = RequestResponseFrame::Constants::OFFSET_BYTE_START;
                    }
                    break;
            }
        }
    }

    void write(const RequestResponseFrame& frame) {
        _listener(frame);
        sendBytes(frame.data(), frame.size());
        process();
    }

private:
    Handler _handler, _listener;
    ReadState _readState = ReadState::WaitingForStart;
    size_t _readOffset = RequestResponseFrame::Constants::OFFSET_BYTE_START;
    RequestResponseFrame _readFrame;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponse_Builder {
public:
    RequestResponse_Builder() {
        _request.setAddress(RequestResponseFrame::Constants::VALUE_ADDRESS_HOST);
    }
    RequestResponse_Builder& setCommand(const uint8_t cmd) {
        _request.setCommand(cmd);
        return *this;
    }
    RequestResponse_Builder& setResponseCount(const size_t count) {
        _responseCount = count;
        return *this;
    }
    const RequestResponseFrame& getRequest() {
        return _request.finalize();
    }
    size_t getResponseCount() const {
        return _responseCount;
    }
private:
    RequestResponseFrame _request;
    size_t _responseCount = 1;
};

// -----------------------------------------------------------------------------------------------

class RequestResponse {
public:
    using Builder = RequestResponse_Builder;

    RequestResponse(RequestResponse_Builder& builder)
        : _request(builder.getRequest()), _responsesExpected(builder.getResponseCount()), _responsesReceived(0) {}
    virtual ~RequestResponse() = default;
    uint8_t getCommand() const {
        return _request.getCommand();
    }
    bool isValid() const {
        return _valid;
    }
    virtual bool isRequestable() const {
        return true;
    }
    virtual RequestResponseFrame prepareRequest() {
        _responsesReceived = 0;
        return _request;
    }
    bool processResponse(const RequestResponseFrame& frame) {
        if (_responsesReceived++ < _responsesExpected && frame.getUInt8(0) == _responsesReceived)
            return processFrame(frame, _responsesReceived);
        else return false;
    }
protected:
    bool setValid(const bool v = true) {
        return (_valid = v);
    }
    virtual bool processFrame(const RequestResponseFrame& frame, const size_t number) {
        return setValid();
    }
    void setResponseCount(const size_t count) {
        _responsesExpected = count;
    }
private:
    bool _valid = false;
    RequestResponseFrame _request;
    size_t _responsesExpected, _responsesReceived;
};

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
    using RequestResponse::setResponseCount;
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
        setResponseCount(LENGTH);
    }
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    using RequestResponseCommand<COMMAND>::setResponseCount;
    bool processFrame(const RequestResponseFrame& frame, const size_t frameNum) override {
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

typedef struct {
    uint8_t year;    // 0-99 + 2000
    uint8_t month;
    uint8_t day;
    String toString() const {
        return String("20") + (year < 10 ? "0" : "") + String(year) + (month < 10 ? "0" : "") + String(month) + (day < 10 ? "0" : "") + String(day);
    }
} FrameTypeDateYMD;

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

    template<typename T>
    static typename std::enable_if<std::is_enum<T>::value, bool>::type
    decode(const RequestResponseFrame& frame, size_t offset, T* value) {
        *value = static_cast<T>(frame.getUInt8(offset));
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
    double packCapacityAh = 0.0f;
    double nominalCellVoltage = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &packCapacityAh, 1000.0) && FrameContentDecoder::decode(frame, 4, &nominalCellVoltage, 1000.0));
    }
};

class RequestResponse_BMS_HARDWARE_CONFIG : public RequestResponseCommand<0x51> {
public:
    uint8_t boards = 0;
    std::array<uint8_t, 3> cells{}, sensors{};
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &boards) && FrameContentDecoder::decode<3>(frame, 1, &cells) && FrameContentDecoder::decode<3>(frame, 4, &sensors));
    }
};

class RequestResponse_BATTERY_STAT : public RequestResponseCommand<0x52> {    // XXX TBC
public:
    double cumulativeChargeAh = 0.0f;
    double cumulativeDischargeAh = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &cumulativeChargeAh) && FrameContentDecoder::decode(frame, 4, &cumulativeDischargeAh));
    }
};

class RequestResponse_BATTERY_INFO : public RequestResponseCommand<0x53> {    // XXX TBC
public:
    uint8_t mode = 0;
    uint8_t type = 0;
    FrameTypeDateYMD productionDate{};
    uint16_t automaticSleepSec = 0;
    uint8_t unknown1 = 0, unknown2 = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &mode) && FrameContentDecoder::decode(frame, 1, &type) && FrameContentDecoder::decode(frame, 2, &productionDate) && FrameContentDecoder::decode_Time_s(frame, 5, &automaticSleepSec) && FrameContentDecoder::decode(frame, 6, &unknown1) && FrameContentDecoder::decode(frame, 7, &unknown2));
    }
};

using RequestResponse_BMS_FIRMWARE_INDEX = RequestResponse_TYPE_STRING<0x54, 1>;

using RequestResponse_BATTERY_CODE = RequestResponse_TYPE_STRING<0x57, 5>;

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND, typename T, int S, auto DECODER>
class RequestResponse_TYPE_THRESHOLD_MINMAX : public RequestResponseCommand<COMMAND> {
public:
    T maxL1 = T(0), maxL2 = T(0);
    T minL1 = T(0), minL2 = T(0);
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            DECODER(frame, S * 0, &maxL1) && DECODER(frame, S * 1, &maxL2) && DECODER(frame, S * 2, &minL1) && DECODER(frame, S * 3, &minL2));
    }
};

using RequestResponse_CELL_VOLTAGES_THRESHOLDS = RequestResponse_TYPE_THRESHOLD_MINMAX<0x59, float, 2, FrameContentDecoder::decode_Voltage_m>;
using RequestResponse_PACK_VOLTAGES_THRESHOLDS = RequestResponse_TYPE_THRESHOLD_MINMAX<0x5A, float, 2, FrameContentDecoder::decode_Voltage_d>;
using RequestResponse_PACK_CURRENTS_THRESHOLDS = RequestResponse_TYPE_THRESHOLD_MINMAX<0x5B, float, 2, FrameContentDecoder::decode_Current_d>;

class RequestResponse_PACK_TEMPERATURE_THRESHOLDS : public RequestResponseCommand<0x5C> {
public:
    int8_t chargeMaxL1C = 0, chargeMaxL2C = 0;
    int8_t chargeMinL1C = 0, chargeMinL2C = 0;
    int8_t dischargeMaxL1C = 0, dischargeMaxL2C = 0;
    int8_t dischargeMinL1C = 0, dischargeMinL2C = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Temperature(frame, 0, &chargeMaxL1C) && FrameContentDecoder::decode_Temperature(frame, 1, &chargeMaxL2C) && FrameContentDecoder::decode_Temperature(frame, 2, &chargeMinL1C) && FrameContentDecoder::decode_Temperature(frame, 3, &chargeMinL2C) && FrameContentDecoder::decode_Temperature(frame, 4, &dischargeMaxL1C) && FrameContentDecoder::decode_Temperature(frame, 5, &dischargeMaxL2C) && FrameContentDecoder::decode_Temperature(frame, 6, &dischargeMinL1C) && FrameContentDecoder::decode_Temperature(frame, 7, &dischargeMinL2C));
    }
};

using RequestResponse_PACK_SOC_THRESHOLDS = RequestResponse_TYPE_THRESHOLD_MINMAX<0x5D, float, 2, FrameContentDecoder::decode_Percent_d>;

class RequestResponse_CELL_SENSORS_THRESHOLDS : public RequestResponseCommand<0x5E> {
public:
    float voltageDiffL1V = 0.0f, voltageDiffL2V = 0.0f;
    int8_t temperatureDiffL1C = 0, temperatureDiffL2C = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Voltage_m(frame, 0, &voltageDiffL1V) && FrameContentDecoder::decode_Voltage_m(frame, 2, &voltageDiffL2V) && FrameContentDecoder::decode_Temperature(frame, 4, &temperatureDiffL1C) && FrameContentDecoder::decode_Temperature(frame, 5, &temperatureDiffL2C));
    }
};

class RequestResponse_CELL_BALANCES_THRESHOLDS : public RequestResponseCommand<0x5F> {
public:
    float voltageEnableThreshold = 0.0f;
    float voltageAcceptableDifferential = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Voltage_m(frame, 0, &voltageEnableThreshold) && FrameContentDecoder::decode_Voltage_m(frame, 2, &voltageAcceptableDifferential));
    }
};

class RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS : public RequestResponseCommand<0x60> {
public:
    float currentShutdownA = 0.0f;
    float currentSamplingR = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &currentShutdownA) && FrameContentDecoder::decode(frame, 2, &currentSamplingR, 1000.0f));
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponse_BMS_RTC : public RequestResponseCommand<0x61> {    // XXX TBC
public:
    uint32_t dateTime1 = 0;
    uint32_t dateTime2 = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
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
    float voltage = 0.0f;
    float current = 0.0f;
    float SOC = 0.0f;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Voltage_d(frame, 0, &voltage) && FrameContentDecoder::decode_Current_d(frame, 4, &current) && FrameContentDecoder::decode_Percent_d(frame, 6, &SOC));
    }
};

template<uint8_t COMMAND, typename T, int S, auto DECODER>
class RequestResponse_TYPE_VALUE_MINMAX : public RequestResponseCommand<COMMAND> {
public:
    T maxValue = T(0);
    uint8_t maxCellNumber = 0;
    T minValue = T(0);
    uint8_t minCellNumber = 0;
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            DECODER(frame, 0, &maxValue) && FrameContentDecoder::decode(frame, S, &maxCellNumber) && DECODER(frame, S + 1, &minValue) && FrameContentDecoder::decode(frame, S + 1 + S, &minCellNumber));
    }
};

using RequestResponse_CELL_VOLTAGES_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x91, float, 2, FrameContentDecoder::decode_Voltage_m>;

using RequestResponse_CELL_TEMPERATURES_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x92, int8_t, 1, FrameContentDecoder::decode_Temperature>;

enum class ChargeState : uint8_t { Stationary = 0x00,
                                   Charge = 0x01,
                                   Discharge = 0x02 };
String toString(ChargeState status) {
    switch (status) {
        case ChargeState::Stationary: return "stationary";
        case ChargeState::Charge: return "charge";
        case ChargeState::Discharge: return "discharge";
        default: return "unknown";
    }
}

class RequestResponse_MOSFET_STATUS : public RequestResponseCommand<0x93> {
public:
    ChargeState state = ChargeState::Stationary;
    bool mosChargeState = false;
    bool mosDischargeState = false;
    uint8_t bmsLifeCycle = 0;
    double residualCapacityAh = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &state) && FrameContentDecoder::decode(frame, 1, &mosChargeState) && FrameContentDecoder::decode(frame, 2, &mosDischargeState) && FrameContentDecoder::decode(frame, 3, &bmsLifeCycle) && FrameContentDecoder::decode(frame, 4, &residualCapacityAh, 1000.0));
    }
};

class RequestResponse_PACK_INFORMATION : public RequestResponseCommand<0x94> {
public:
    uint8_t numberOfCells = 0;
    uint8_t numberOfSensors = 0;
    bool chargerStatus = false;
    bool loadStatus = false;
    std::array<bool, 8> dioStates{};
    uint16_t cycles = 0;
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &numberOfCells) && FrameContentDecoder::decode(frame, 1, &numberOfSensors) && FrameContentDecoder::decode(frame, 2, &chargerStatus) && FrameContentDecoder::decode(frame, 3, &loadStatus) && FrameContentDecoder::decode(frame, 4, &dioStates) && FrameContentDecoder::decode(frame, 5, &cycles));
    }
};

template<uint8_t COMMAND, typename T, int S, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, auto DECODER>
class RequestResponse_TYPE_ARRAY : public RequestResponseCommand<COMMAND> {
public:
    std::vector<T> values;
    bool setCount(const size_t count) {
        if (count > 0 && count < ITEMS_MAX) {
            values.resize(count);
            setResponseCount((count / ITEMS_PER_FRAME) + 1);
            return true;
        }
        return false;
    }
    bool isRequestable() const override {
        return !values.empty();
    }
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    using RequestResponseCommand<COMMAND>::setResponseCount;
    bool processFrame(const RequestResponseFrame& frame, const size_t frameNum) override {
        if (frame.getUInt8(0) != frameNum || frame.getUInt8(0) > (values.size() / ITEMS_PER_FRAME)) return false;
        for (size_t i = 0; i < ITEMS_PER_FRAME && ((frameNum * ITEMS_PER_FRAME) + i) < values.size(); i++)
            if (!DECODER(frame, 1 + i * S, &values[(frameNum * ITEMS_PER_FRAME) + i]))
                return false;    // XXX underlying will pass through the next frame though ...
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
    bool show = false;
    std::bitset<NUM_FAILURE_CODES> bits;
    size_t count = 0;
    size_t getFailureList(const char** output, const size_t maxFailures) const {
        size_t c = 0;
        for (size_t i = 0; i < NUM_FAILURE_CODES && count < maxFailures; ++i)
            if (bits[i])
                output[c++] = FAILURE_DESCRIPTIONS[i];
        return c;
    }
protected:
    bool processFrame(const RequestResponseFrame& frame, const size_t) override {
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

class RequestResponseManager {
public:

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void onValidResponse(RequestResponse* response) = 0;
    };

    void registerListener(Listener* listener) {
        _listeners.push_back(listener);
    }
    void unregisterListener(Listener* listener) {
        auto pos = std::find(_listeners.begin(), _listeners.end(), listener);
        if (pos != _listeners.end())
            _listeners.erase(pos);
    }

    bool receiveFrame(const RequestResponseFrame& frame) {
        auto it = _requestsMap.find(frame.getCommand());
        if (it != _requestsMap.end() && it->second->processResponse(frame)) {
            if (it->second->isValid())
                notifyListeners(it->second);
            return true;
        }
        return false;
    }

    explicit RequestResponseManager(const std::vector<RequestResponse*>& requests)
        : _requests(requests) {
        for (auto request : _requests)
            _requestsMap[request->getCommand()] = request;
    }

private:
    void notifyListeners(RequestResponse* response) {
        for (auto listener : _listeners)
            listener->onValidResponse(response);
    }

    const std::vector<RequestResponse*> _requests;
    std::map<uint8_t, RequestResponse*> _requestsMap;
    std::vector<Listener*> _listeners;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

enum class Capabilities {
    None = 0,
    Managing = 1 << 0,
    Balancing = 1 << 1,
    TemperatureSensing = 1 << 3,
    All = Managing | Balancing | TemperatureSensing
};
enum class Categories {
    None = 0,
    Information = 1 << 0,
    Thresholds = 1 << 1,
    Status = 1 << 2,
    Diagnostics = 1 << 3,
    Commands = 1 << 4,
    All = Information | Thresholds | Status | Diagnostics | Commands
};

inline Capabilities operator|(Capabilities a, Capabilities b) {
    return static_cast<Capabilities>(static_cast<int>(a) | static_cast<int>(b));
}
inline Capabilities operator&(Capabilities a, Capabilities b) {
    return static_cast<Capabilities>(static_cast<int>(a) & static_cast<int>(b));
}
inline Categories operator|(Categories a, Categories b) {
    return static_cast<Categories>(static_cast<int>(a) | static_cast<int>(b));
}
inline Categories operator&(Categories a, Categories b) {
    return static_cast<Categories>(static_cast<int>(a) & static_cast<int>(b));
}

inline String toString(Capabilities capability) {
    switch (capability) {
        case Capabilities::None: return "None";
        case Capabilities::Managing: return "Managing";
        case Capabilities::Balancing: return "Balancing";
        case Capabilities::TemperatureSensing: return "TemperatureSensing";
        case Capabilities::All: return "All";
        default: return "Unknown";
    }
}
inline String toString(Categories category) {
    switch (category) {
        case Categories::None: return "None";
        case Categories::Information: return "Information";
        case Categories::Thresholds: return "Thresholds";
        case Categories::Status: return "Status";
        case Categories::Diagnostics: return "Diagnostics";
        case Categories::Commands: return "Commands";
        case Categories::All: return "All";
        default: return "Unknown";
    }
}

// -----------------------------------------------------------------------------------------------

class Interface;
void dumpDebug(const Interface&);

class Interface {

public:
    using Capabilities = daly_bms::Capabilities;
    using Categories = daly_bms::Categories;

    typedef struct {
        String id;
        Capabilities capabilities;
        Categories categories;
    } Config;

    using Connector = RequestResponseFrame::Receiver;

    struct Information {    // static, unofficial
        RequestResponse_BMS_HARDWARE_CONFIG hardware_config;
        RequestResponse_BMS_HARDWARE_VERSION hardware_version;
        RequestResponse_BMS_FIRMWARE_INDEX firmware_index;
        RequestResponse_BMS_SOFTWARE_VERSION software_version;
        RequestResponse_BATTERY_RATINGS battery_ratings;
        RequestResponse_BATTERY_CODE battery_code;
        RequestResponse_BATTERY_INFO battery_info;
        RequestResponse_BATTERY_STAT battery_stat;
        RequestResponse_BMS_RTC rtc;
    } information;
    struct Thresholds {    // static, unofficial
        RequestResponse_PACK_VOLTAGES_THRESHOLDS pack_voltages;
        RequestResponse_PACK_CURRENTS_THRESHOLDS pack_currents;
        RequestResponse_PACK_TEMPERATURE_THRESHOLDS pack_temperatures;
        RequestResponse_PACK_SOC_THRESHOLDS pack_soc;
        RequestResponse_CELL_VOLTAGES_THRESHOLDS cell_voltages;
        RequestResponse_CELL_SENSORS_THRESHOLDS cell_sensors;
        RequestResponse_CELL_BALANCES_THRESHOLDS cell_balances;
        RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS pack_shortcircuit;
    } thresholds;
    struct Status {
        RequestResponse_PACK_STATUS pack;
        RequestResponse_CELL_VOLTAGES_MINMAX cell_voltages;
        RequestResponse_CELL_TEMPERATURES_MINMAX cell_temperatures;
        RequestResponse_MOSFET_STATUS fets;
        RequestResponse_PACK_INFORMATION info;
        RequestResponse_FAILURE_STATUS failures;
    } status;
    struct Diagnostics {
        RequestResponse_CELL_VOLTAGES voltages;
        RequestResponse_CELL_TEMPERATURES temperatures;
        RequestResponse_CELL_BALANCES balances;
    } diagnostics;
    struct Commands {    // unofficial
        RequestResponse_BMS_RESET reset;
        RequestResponse_MOSFET_DISCHARGE discharge;
        RequestResponse_MOSFET_CHARGE charge;
    } commands;

    std::map<Categories, std::vector<RequestResponse*>> _requests;
    struct RequestResponseMetadata {
        Categories category;
        Capabilities capabilities;
        RequestResponse* request;
    };
    std::vector<RequestResponseMetadata> _requestResponseMap;

    std::vector<RequestResponse*> buildRequestResponses(const Capabilities capabilities) {
        std::vector<RequestResponse*> r;
        for (const auto& item : _requestResponseMap)
            if ((item.capabilities & capabilities) != Capabilities::None)
                _requests[item.category].push_back(item.request);
        for (const auto& [category, requests] : _requests)
            r.insert(r.end(), requests.begin(), requests.end());
        return r;
    }

    explicit Interface(const Config& conf, Connector& connector)
        : thresholds{}, status{}, diagnostics{}, commands{},
          _requestResponseMap({

              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, &information.hardware_config },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, &information.hardware_version },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, &information.firmware_index },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, &information.software_version },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, &information.battery_ratings },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, &information.battery_code },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, &information.battery_info },
              { Categories::Information, Capabilities::Managing, &information.battery_stat },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, &information.rtc },

              { Categories::Thresholds, Capabilities::Managing | Capabilities::Balancing, &thresholds.pack_voltages },
              { Categories::Thresholds, Capabilities::Managing | Capabilities::Balancing, &thresholds.pack_currents },
              { Categories::Thresholds, Capabilities::TemperatureSensing, &thresholds.pack_temperatures },
              { Categories::Thresholds, Capabilities::Managing, &thresholds.pack_soc },
              { Categories::Thresholds, Capabilities::Managing | Capabilities::Balancing, &thresholds.cell_voltages },
              { Categories::Thresholds, Capabilities::TemperatureSensing, &thresholds.cell_sensors },
              { Categories::Thresholds, Capabilities::Balancing, &thresholds.cell_balances },
              { Categories::Thresholds, Capabilities::Managing, &thresholds.pack_shortcircuit },

              { Categories::Status, Capabilities::Managing, &status.pack },
              { Categories::Status, Capabilities::Managing | Capabilities::Balancing, &status.cell_voltages },
              { Categories::Status, Capabilities::TemperatureSensing, &status.cell_temperatures },
              { Categories::Status, Capabilities::Managing, &status.fets },
              { Categories::Status, Capabilities::Managing | Capabilities::Balancing, &status.info },
              { Categories::Status, Capabilities::Managing, &status.failures },

              { Categories::Diagnostics, Capabilities::Managing | Capabilities::Balancing, &diagnostics.voltages },
              { Categories::Diagnostics, Capabilities::TemperatureSensing, &diagnostics.temperatures },
              { Categories::Diagnostics, Capabilities::Balancing, &diagnostics.balances },

              { Categories::Commands, Capabilities::Managing | Capabilities::Balancing, &commands.reset },
              { Categories::Commands, Capabilities::Managing, &commands.charge },
              { Categories::Commands, Capabilities::Managing, &commands.discharge }

          }),
          config(conf), _connector(connector), _manager(buildRequestResponses(config.capabilities)) {

        struct TemporaryCountListener : RequestResponseManager::Listener {
            Diagnostics& diagnostics;
            RequestResponseManager& manager;
            const Status& status;
            TemporaryCountListener(Diagnostics& d, RequestResponseManager& m, const Status& s)
                : diagnostics(d), manager(m), status(s) {}
            void onValidResponse(RequestResponse* response) override {
                if (response->getCommand() == status.info) {
                    diagnostics.voltages.setCount(status.info.numberOfCells);
                    diagnostics.temperatures.setCount(status.info.numberOfSensors);
                    diagnostics.balances.setCount(status.info.numberOfCells);
                    manager.unregisterListener(this);
                    delete this;
                }
            }
        };
        _manager.registerListener(new TemporaryCountListener(diagnostics, _manager, status));

        _connector.registerHandler([this](const RequestResponseFrame& frame) {
            DEBUG_PRINTF("DalyBMS<%s>: recv: %s\n", config.id, frame.toString().c_str());
            _manager.receiveFrame(frame);
        });
        _connector.registerListener([this](const RequestResponseFrame& frame) {
            DEBUG_PRINTF("DalyBMS<%s>: send: %s\n", config.id, frame.toString().c_str());
        });
    }

    void begin() {
        DEBUG_PRINTF("DalyBMS<%s>: begin\n", config.id);
        _connector.begin();
        requestInitial();
    }
    void loop() {
        _connector.process();
    }

    void issue(RequestResponse* request) {
        if (request->isRequestable())
            _connector.write(request->prepareRequest());
    }

    void requestStatus() {
        request(Categories::Status);
    }
    void requestDiagnostics() {
        request(Categories::Diagnostics);
    }
protected:
    void requestInformation() {
        request(Categories::Information);
    }
    void requestThresholds() {
        request(Categories::Thresholds);
    }
public:
    void request(const Categories category) {
        if ((config.categories & category) == Categories::None) return;
        DEBUG_PRINTF("DalyBMS<%s>: request%s\n", config.id, toString(category).c_str());
        auto it = _requests.find(category);
        if (it != _requests.end())
            for (auto* r : it->second)
                issue(r);
    }
    void requestInitial() {
        DEBUG_PRINTF("DalyBMS<%s>: requestInitial\n", config.id);
        for (auto category : { Categories::Information, Categories::Thresholds, Categories::Status, Categories::Diagnostics, Categories::Commands })
            request(category);
    }
    void dumpDebug() const {
        return daly_bms::dumpDebug(*this);
    }

private:
    friend void daly_bms::dumpDebug(const Interface&);
    friend bool convertToJson(const Interface&, JsonVariant);
    const Config& config;
    Connector& _connector;
    RequestResponseManager _manager;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class HardwareSerialConnector : public Interface::Connector {
public:
    explicit HardwareSerialConnector(HardwareSerial& serial)
        : _serial(serial) {
    }
protected:
    void begin() override {
    }
    bool getByte(uint8_t* byte) override {
        if (_serial.available() > 0) {
            *byte = _serial.read();
            return true;
        } else DEBUG_PRINTF("no data\n");
        return false;
    }
    bool sendBytes(const uint8_t* data, const size_t size) override {
        return _serial.write(data, size) == size;
    }
private:
    HardwareSerial& _serial;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <type_traits>

template<typename T> struct TypeName { static constexpr const char* name = "unknown"; };
template<> struct TypeName<RequestResponse_BMS_HARDWARE_CONFIG> { static constexpr const char* name = "hardware_config"; };
template<> struct TypeName<RequestResponse_BMS_HARDWARE_VERSION> { static constexpr const char* name = "hardware_version"; };
template<> struct TypeName<RequestResponse_BMS_FIRMWARE_INDEX> { static constexpr const char* name = "firmware_index"; };
template<> struct TypeName<RequestResponse_BMS_SOFTWARE_VERSION> { static constexpr const char* name = "software_version"; };
template<> struct TypeName<RequestResponse_BATTERY_RATINGS> { static constexpr const char* name = "battery_ratings"; };
template<> struct TypeName<RequestResponse_BATTERY_CODE> { static constexpr const char* name = "battery_code"; };
template<> struct TypeName<RequestResponse_BATTERY_INFO> { static constexpr const char* name = "battery_info"; };
template<> struct TypeName<RequestResponse_BATTERY_STAT> { static constexpr const char* name = "battery_stat"; };
template<> struct TypeName<RequestResponse_BMS_RTC> { static constexpr const char* name = "rtc"; };
template<> struct TypeName<RequestResponse_PACK_VOLTAGES_THRESHOLDS> { static constexpr const char* name = "pack_voltages"; };
template<> struct TypeName<RequestResponse_PACK_CURRENTS_THRESHOLDS> { static constexpr const char* name = "pack_currents"; };
template<> struct TypeName<RequestResponse_PACK_TEMPERATURE_THRESHOLDS> { static constexpr const char* name = "pack_temperatures"; };
template<> struct TypeName<RequestResponse_PACK_SOC_THRESHOLDS> { static constexpr const char* name = "pack_soc"; };
template<> struct TypeName<RequestResponse_CELL_VOLTAGES_THRESHOLDS> { static constexpr const char* name = "cell_voltages"; };
template<> struct TypeName<RequestResponse_CELL_SENSORS_THRESHOLDS> { static constexpr const char* name = "cell_sensors"; };
template<> struct TypeName<RequestResponse_CELL_BALANCES_THRESHOLDS> { static constexpr const char* name = "cell_balances"; };
template<> struct TypeName<RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS> { static constexpr const char* name = "pack_shortcircuit"; };
template<> struct TypeName<RequestResponse_PACK_STATUS> { static constexpr const char* name = "pack"; };
template<> struct TypeName<RequestResponse_CELL_VOLTAGES_MINMAX> { static constexpr const char* name = "cell_voltages"; };
template<> struct TypeName<RequestResponse_CELL_TEMPERATURES_MINMAX> { static constexpr const char* name = "cell_temperatures"; };
template<> struct TypeName<RequestResponse_MOSFET_STATUS> { static constexpr const char* name = "fets"; };
template<> struct TypeName<RequestResponse_PACK_INFORMATION> { static constexpr const char* name = "info"; };
template<> struct TypeName<RequestResponse_FAILURE_STATUS> { static constexpr const char* name = "failures"; };
template<> struct TypeName<RequestResponse_CELL_VOLTAGES> { static constexpr const char* name = "voltages"; };
template<> struct TypeName<RequestResponse_CELL_TEMPERATURES> { static constexpr const char* name = "temperatures"; };
template<> struct TypeName<RequestResponse_CELL_BALANCES> { static constexpr const char* name = "balances"; };

template<typename T> constexpr const char* getName(const T&) {
    return TypeName<T>::name;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND, int LENGTH>
bool convertToJson(const RequestResponse_TYPE_STRING<COMMAND, LENGTH>& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst.set(src.string);
    return true;
}
template<uint8_t COMMAND, typename T, int S, auto DECODER>
bool convertToJson(const RequestResponse_TYPE_THRESHOLD_MINMAX<COMMAND, T, S, DECODER>& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["maxL1"] = src.maxL1;
    dst["maxL2"] = src.maxL2;
    dst["minL1"] = src.minL1;
    dst["minL2"] = src.minL2;
    return true;
}
template<uint8_t COMMAND, typename T, int S, auto DECODER>
bool convertToJson(const RequestResponse_TYPE_VALUE_MINMAX<COMMAND, T, S, DECODER>& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    JsonObject max = dst["max"].to<JsonObject>();
    max["value"] = src.maxValue;
    max["cell"] = src.maxCellNumber;
    JsonObject min = dst["min"].to<JsonObject>();
    min["value"] = src.minValue;
    min["cell"] = src.minCellNumber;
    return true;
}
template<uint8_t COMMAND, typename T, int S, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, auto DECODER>
bool convertToJson(const RequestResponse_TYPE_ARRAY<COMMAND, T, S, ITEMS_MAX, ITEMS_PER_FRAME, DECODER>& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    JsonArray values = dst["values"].to<JsonArray>();
    for (const auto& value : src.values)
        values.add(value);
    return true;
}
//
bool convertToJson(const RequestResponse_BMS_HARDWARE_CONFIG& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["boards"] = src.boards;
    JsonArray cells = dst["cells"].to<JsonArray>();
    JsonArray sensors = dst["sensors"].to<JsonArray>();
    for (int i = 0; i < 3; i++) {
        cells.add(src.cells[i]);
        sensors.add(src.sensors[i]);
    }
    return true;
}
bool convertToJson(const RequestResponse_BATTERY_RATINGS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["packCapacityAh"] = src.packCapacityAh;
    dst["nominalCellVoltage"] = src.nominalCellVoltage;
    return true;
}
bool convertToJson(const RequestResponse_BATTERY_INFO& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["mode"] = src.mode;
    dst["type"] = src.type;
    dst["productionDate"] = src.productionDate.toString();
    dst["automaticSleepSec"] = src.automaticSleepSec;
    return true;
}
bool convertToJson(const RequestResponse_BATTERY_STAT& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["cumulativeChargeAh"] = src.cumulativeChargeAh;
    dst["cumulativeDischargeAh"] = src.cumulativeDischargeAh;
    return true;
}
bool convertToJson(const RequestResponse_BMS_RTC& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["dateTime1"] = src.dateTime1;
    dst["dateTime2"] = src.dateTime2;
    return true;
}
bool convertToJson(const RequestResponse_PACK_TEMPERATURE_THRESHOLDS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    JsonObject charge = dst["charge"].to<JsonObject>();
    charge["maxL1"] = src.chargeMaxL1C;
    charge["maxL2"] = src.chargeMaxL2C;
    charge["minL1"] = src.chargeMinL1C;
    charge["minL2"] = src.chargeMinL2C;
    JsonObject discharge = dst["discharge"].to<JsonObject>();
    discharge["maxL1"] = src.dischargeMaxL1C;
    discharge["maxL2"] = src.dischargeMaxL2C;
    discharge["minL1"] = src.dischargeMinL1C;
    discharge["minL2"] = src.dischargeMinL2C;
    return true;
}
bool convertToJson(const RequestResponse_CELL_SENSORS_THRESHOLDS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    JsonObject voltage = dst["voltage"].to<JsonObject>();
    voltage["diffL1"] = src.voltageDiffL1V;
    voltage["diffL2"] = src.voltageDiffL2V;
    JsonObject temperature = dst["temperature"].to<JsonObject>();
    temperature["diffL1"] = src.temperatureDiffL1C;
    temperature["diffL2"] = src.temperatureDiffL2C;
    return true;
}
bool convertToJson(const RequestResponse_CELL_BALANCES_THRESHOLDS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["voltageEnableThreshold"] = src.voltageEnableThreshold;
    dst["voltageAcceptableDifferential"] = src.voltageAcceptableDifferential;
    return true;
}
bool convertToJson(const RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["currentShutdownA"] = src.currentShutdownA;
    dst["currentSamplingR"] = src.currentSamplingR;
    return true;
}
bool convertToJson(const RequestResponse_PACK_STATUS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["voltage"] = src.voltage;
    dst["current"] = src.current;
    dst["charge"] = src.SOC;
    return true;
}
bool convertToJson(const RequestResponse_MOSFET_STATUS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["state"] = toString(src.state);
    dst["mosCharge"] = src.mosChargeState;
    dst["mosDischarge"] = src.mosDischargeState;
    dst["bmsLifeCycle"] = src.bmsLifeCycle;
    dst["residualCapacityAh"] = src.residualCapacityAh;
    return true;
}
bool convertToJson(const RequestResponse_PACK_INFORMATION& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["cells"] = src.numberOfCells;
    dst["sensors"] = src.numberOfSensors;
    dst["charger"] = src.chargerStatus;
    dst["load"] = src.loadStatus;
    JsonArray dioStates = dst["dioStates"].to<JsonArray>();
    for (const auto& state : src.dioStates)
        dioStates.add(state);
    dst["cycles"] = src.cycles;
    return true;
}
bool convertToJson(const RequestResponse_FAILURE_STATUS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["show"] = src.show;
    dst["count"] = src.count;
    if (src.count > 0) {
        JsonArray active = dst["active"].to<JsonArray>();
        const char* failures[src.count];
        for (size_t i = 0, c = src.getFailureList(failures, src.count); i < c; i++)
            active.add(failures[i]);
    }
    return true;
}
template<typename T>
void addToJson(T flags, JsonArray&& arr) {
    for (T flag = static_cast<T>(1); flag < T::All; flag = static_cast<T>(static_cast<int>(flag) << 1))
        if ((flags & flag) != T::None) arr.add(toString(flag));
}
bool convertToJson(const daly_bms::Interface::Config& src, JsonVariant dst) {
    dst["id"] = src.id;
    addToJson(src.capabilities, dst["capabilities"].to<JsonArray>());
    addToJson(src.categories, dst["categories"].to<JsonArray>());
    return true;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<typename T>
void debugDump(const T&) {
    DEBUG_PRINTF("<unknown>\n");
}
template<uint8_t COMMAND, int LENGTH>
void debugDump(const RequestResponse_TYPE_STRING<COMMAND, LENGTH>& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("%s\n", src.string.c_str());
}
template<uint8_t COMMAND, typename T, int S, auto DECODER>
void debugDump(const RequestResponse_TYPE_THRESHOLD_MINMAX<COMMAND, T, S, DECODER>& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("max L1=%.1f,L2=%.1f, min L1=%.1f,L2=%.1f\n",    // XXX depending on T, but is always float
                 src.maxL1, src.maxL2,
                 src.minL1, src.minL2);
}
template<uint8_t COMMAND, typename T, int S, auto DECODER>
void debugDump(const RequestResponse_TYPE_VALUE_MINMAX<COMMAND, T, S, DECODER>& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("max=%.3f (#%d), min=%.3f (#%d)\n",    // XXX depending on T, is float, int8_t
                 src.maxValue, src.maxCellNumber,
                 src.minValue, src.minCellNumber);
}
template<uint8_t COMMAND, typename T, int S, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, auto DECODER>
void debugDump(const RequestResponse_TYPE_ARRAY<COMMAND, T, S, ITEMS_MAX, ITEMS_PER_FRAME, DECODER>& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("%u /", src.values.size());
    for (const auto& v : src.values)
        DEBUG_PRINTF(" %d", v);    // XXX depending on T, is float, int8_t, uint8_t
    DEBUG_PRINTF("\n");
}
//
void debugDump(const RequestResponse_BMS_HARDWARE_CONFIG& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("boards=%d, cells=%d,%d,%d, sensors=%d,%d,%d\n",
                 src.boards,
                 src.cells[0], src.cells[1], src.cells[2],
                 src.sensors[0], src.sensors[1], src.sensors[2]);
}
void debugDump(const RequestResponse_BATTERY_RATINGS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("capacity=%.1fAh, nominalCellVoltage=%.1fV\n",
                 src.packCapacityAh, src.nominalCellVoltage);
}
void debugDump(const RequestResponse_BATTERY_STAT& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("charge=%.1fAh, discharge=%.1fAh\n",
                 src.cumulativeChargeAh, src.cumulativeDischargeAh);
}
void debugDump(const RequestResponse_BATTERY_INFO& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("mode=%d, type=%d, date=%s, sleep=%d\n",
                 src.mode, src.type,
                 src.productionDate.toString().c_str(),
                 src.automaticSleepSec);
}
void debugDump(const RequestResponse_PACK_TEMPERATURE_THRESHOLDS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("charge max L1=%dC,L2=%dC min L1=%dC,L2=%dC, discharge max L1=%dC,L2=%dC min L1=%dC,L2=%dC\n",
                 src.chargeMaxL1C, src.chargeMaxL2C,
                 src.chargeMinL1C, src.chargeMinL2C,
                 src.dischargeMaxL1C, src.dischargeMaxL2C,
                 src.dischargeMinL1C, src.dischargeMinL2C);
}
void debugDump(const RequestResponse_CELL_SENSORS_THRESHOLDS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("volt diff L1=%.3fV,L2=%.3fV, temp diff L1=%dC,L2=%dC\n",
                 src.voltageDiffL1V, src.voltageDiffL2V,
                 src.temperatureDiffL1C, src.temperatureDiffL2C);
}
void debugDump(const RequestResponse_CELL_BALANCES_THRESHOLDS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("enable=%.3fV, acceptable=%.3fV\n",
                 src.voltageEnableThreshold,
                 src.voltageAcceptableDifferential);
}
void debugDump(const RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("shutdown=%.1fA, sampling=%.3fR\n",
                 src.currentShutdownA, src.currentSamplingR);
}
void debugDump(const RequestResponse_BMS_RTC& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("dt1=%u, dt2=%u\n", src.dateTime1, src.dateTime2);
}
void debugDump(const RequestResponse_PACK_STATUS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("%.1f volts, %.1f amps, %.1f percent\n",
                 src.voltage, src.current, src.SOC);
}
void debugDump(const RequestResponse_MOSFET_STATUS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("state=%s, MOS charge=%s, discharge=%s, cycle=%d, capacity=%.1fAh\n",
                 toString(src.state).c_str(),
                 src.mosChargeState ? "on" : "off",
                 src.mosDischargeState ? "on" : "off",
                 src.bmsLifeCycle,
                 src.residualCapacityAh);
}
void debugDump(const RequestResponse_PACK_INFORMATION& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("cells=%d, sensors=%d, charger=%s, load=%s, cycles=%d\n",
                 src.numberOfCells, src.numberOfSensors,
                 src.chargerStatus ? "on" : "off",
                 src.loadStatus ? "on" : "off",
                 src.cycles);
}
void debugDump(const RequestResponse_FAILURE_STATUS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("show=%s, count=%d",
                 src.show ? "yes" : "no", src.count);
    if (src.count > 0) {
        DEBUG_PRINTF(", active=[");
        const char* failures[src.count];
        for (size_t i = 0, c = src.getFailureList(failures, src.count); i < c; i++)
            DEBUG_PRINTF("%s%s", i == 0 ? "" : ",", failures[i]);
        DEBUG_PRINTF("]");
    }
    DEBUG_PRINTF("\n");
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

bool convertToJson(const Interface& src, JsonVariant dst) {

    //

    const auto category = [&](Categories category) {
        return ((src.config.categories & category) != Categories::None);
    };
    const auto handler = [&](const Categories category) -> JsonVariant {
        return dst[toString(category)];
    };
    const auto process = [&](auto&& handler, const auto& component) {    // XXX need to check if enabled by capabilities
        if (component.isValid())
            handler[getName(component)] = component;
    };
    const auto config = [&](const auto& config) {
        dst["config"] = config;
    };

    //

    config(src.config);
    if (category(Categories::Information)) {
        auto information = handler(Categories::Information);
        process(information, src.information.hardware_config);
        process(information, src.information.hardware_version);
        process(information, src.information.firmware_index);
        process(information, src.information.software_version);
        process(information, src.information.battery_ratings);
        process(information, src.information.battery_code);
        process(information, src.information.battery_info);
        process(information, src.information.battery_stat);
        process(information, src.information.rtc);
    }
    if (category(Categories::Thresholds)) {
        auto thresholds = handler(Categories::Thresholds);
        process(thresholds, src.thresholds.pack_voltages);
        process(thresholds, src.thresholds.pack_currents);
        process(thresholds, src.thresholds.pack_temperatures);
        process(thresholds, src.thresholds.pack_soc);
        process(thresholds, src.thresholds.cell_voltages);
        process(thresholds, src.thresholds.cell_sensors);
        process(thresholds, src.thresholds.cell_balances);
        process(thresholds, src.thresholds.pack_shortcircuit);
    }
    if (category(Categories::Status)) {
        auto status = handler(Categories::Status);
        process(status, src.status.pack);
        process(status, src.status.cell_voltages);
        process(status, src.status.cell_temperatures);
        process(status, src.status.fets);
        process(status, src.status.info);
        process(status, src.status.failures);
    }
    if (category(Categories::Diagnostics)) {
        auto diagnostics = handler(Categories::Diagnostics);
        process(diagnostics, src.diagnostics.voltages);
        process(diagnostics, src.diagnostics.temperatures);
        process(diagnostics, src.diagnostics.balances);
    }
    return true;
}

void dumpDebug(const Interface& src) {

    //

    const auto category = [&](const Categories category) {
        DEBUG_PRINTF("DalyBMS<%s>: %s:\n", src.config.id, toString(category));
        return ((src.config.categories & category) != Categories::None);
    };
    const auto handler = [&](const Categories category) -> const String {
        return toString(category);
    };
    const auto process = [&](auto&&, const auto& component) {    // XXX need to check if enabled by capabilities
        const bool valid = component.isValid();
        DEBUG_PRINTF("  %s: %s", getName(component), valid ? "" : "<Not valid>");
        if (valid) debugDump(component);
        return valid;
    };
    const auto config = [&](const auto& config) {
        DEBUG_PRINTF("DalyBMS<%s>: capabilities=%s, categories=%s\n", config.id, toString(config.capabilities), toString(config.categories));
    };

    //

    config(src.config);
    if (category(Categories::Information)) {
        auto information = handler(Categories::Information);
        process(information, src.information.hardware_config);
        process(information, src.information.hardware_version);
        process(information, src.information.firmware_index);
        process(information, src.information.software_version);
        process(information, src.information.battery_ratings);
        process(information, src.information.battery_code);
        process(information, src.information.battery_info);
        process(information, src.information.battery_stat);
        process(information, src.information.rtc);
    }
    if (category(Categories::Thresholds)) {
        auto thresholds = handler(Categories::Thresholds);
        process(thresholds, src.thresholds.pack_voltages);
        process(thresholds, src.thresholds.pack_currents);
        process(thresholds, src.thresholds.pack_temperatures);
        process(thresholds, src.thresholds.pack_soc);
        process(thresholds, src.thresholds.cell_voltages);
        process(thresholds, src.thresholds.cell_sensors);
        process(thresholds, src.thresholds.cell_balances);
        process(thresholds, src.thresholds.pack_shortcircuit);
    }
    if (category(Categories::Status)) {
        auto status = handler(Categories::Status);
        process(status, src.status.pack);
        process(status, src.status.cell_voltages);
        process(status, src.status.cell_temperatures);
        process(status, src.status.fets);
        process(status, src.status.info);
        process(status, src.status.failures);
    }
    if (category(Categories::Diagnostics)) {
        auto diagnostics = handler(Categories::Diagnostics);
        process(diagnostics, src.diagnostics.voltages);
        process(diagnostics, src.diagnostics.temperatures);
        process(diagnostics, src.diagnostics.balances);
    }
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
