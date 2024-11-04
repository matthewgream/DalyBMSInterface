// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#pragma once

#include "DalyBMSUtilities.hpp"

#include <cstdint>

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

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
        static constexpr uint8_t VALUE_ADDRESS_BMS_MASTER = 0x01;
        static constexpr uint8_t VALUE_ADDRESS_BLUETOOTH_APP = 0x80;
        static constexpr uint8_t VALUE_ADDRESS_GPRS = 0x20;
        static constexpr uint8_t VALUE_ADDRESS_UPPER_COMPUTER = 0x40;
    };

    RequestResponseFrame() {
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
        return _data[Constants::OFFSET_BYTE_START] == Constants::VALUE_BYTE_START && 
        _data[Constants::OFFSET_ADDRESS] == Constants::VALUE_ADDRESS_BMS_MASTER && 
        _data[Constants::OFFSET_SIZE] == Constants::SIZE_DATA && _data[Constants::OFFSET_CHECKSUM] == calculateChecksum();
    }

    //

    inline uint8_t getUInt8(const size_t offset) const {
        validateDataOffset(offset);
        return _data[Constants::SIZE_HEADER + offset];
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
    inline RequestResponseFrame& setUInt8(const size_t offset, const uint8_t value) {
        validateDataOffset(offset);
        _data[Constants::SIZE_HEADER + offset] = value;
        return *this;
    }

    //

    const uint8_t* data() const {
        return _data.data();
    }
    static constexpr size_t size() {
        return Constants::SIZE_FRAME;
    }

    String toString() const {
        return toStringHex (_data.data (), Constants::SIZE_FRAME, " ");
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
        for (size_t i = 0; i < Constants::OFFSET_CHECKSUM; i++)
            sum += _data[i];
        return sum;
    }

    std::array<uint8_t, Constants::SIZE_FRAME> _data{};
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponse_Builder {
public:
    RequestResponse_Builder() {
        _request.setAddress(RequestResponseFrame::Constants::VALUE_ADDRESS_UPPER_COMPUTER);
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
    RequestResponseFrame _request{};
    size_t _responseCount{1};
};

// -----------------------------------------------------------------------------------------------

class RequestResponse {
public:
    using Builder = RequestResponse_Builder;

    RequestResponse(RequestResponse_Builder& builder)
        : _request(builder.getRequest()), _responsesExpected(builder.getResponseCount()) {}
    virtual ~RequestResponse() = default;
    uint8_t getCommand() const {
        return _request.getCommand();
    }
    bool isValid() const {
        return _validState;
    }
    SystemTicks_t valid() const {
        return _validTime;
    }
    virtual bool isRequestable() const {
        return true;
    }
    bool isComplete() const {
        return (_responsesReceived == _responsesExpected);
    }
    virtual RequestResponseFrame prepareRequest() {
        _responsesReceived = 0;
        return _request;
    }
    bool processResponse(const RequestResponseFrame& frame) {
        _validState = false;
        if (++ _responsesReceived <= _responsesExpected && (_responsesExpected == 1 || frame.getUInt8(0) == _responsesReceived))
            return processResponseFrame(frame, _responsesReceived);
        else return false;
    }
    String getName () const {
        return demangle (typeid(*this).name());
    }
protected:
    bool setValid(const bool v = true) {
        if ((_validState = v)) _validTime = systemTicksNow ();
        _responsesReceived = 0;
        return v;
    }
    virtual bool processResponseFrame(const RequestResponseFrame& frame, const size_t number) {
        return setValid();
    }
    void setResponseFrameCount(const size_t count) {
        _responsesExpected = count;
    }
private:
    bool _validState{};
    SystemTicks_t _validTime{};
    RequestResponseFrame _request{};
    size_t _responsesExpected{}, _responsesReceived{};
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

} // namespace daly_bms

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
