
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifndef DALYBMS_FLATFILES
#pragma once
#include "DalyBMSUtilities.hpp"
#include "DalyBMSRequestResponse.hpp"
#include "DalyBMSManager.hpp"
#endif

#include <HardwareSerial.h>

namespace daly_bms {

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class HardwareSerialConnector : public RequestResponseFrame::Receiver {
public:

    static inline constexpr int DEFAULT_SERIAL_BUFFER_RX = 1024, DEFAULT_SERIAL_BUFFER_TX = 512;
    static inline constexpr int DEFAULT_SERIAL_BAUD = 9600;
    static inline constexpr SerialConfig DEFAULT_SERIAL_CONFIG = SERIAL_8N1;

    explicit HardwareSerialConnector(HardwareSerial& serial)
        : _serial(serial) {
    }
protected:
    void begin() override {
    }
    bool readByte(uint8_t* byte) override {
        if (_serial.available() > 0) {
            *byte = _serial.read();
            return true;
        }
        return false;
    }
    bool writeBytes(const uint8_t* data, const size_t size) override {
        return _serial.write(data, size) == size;
    }
private:
    HardwareSerial& _serial;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms
