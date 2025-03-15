
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

struct SerialInterface {
    static inline constexpr int DEFAULT_SERIAL_BUFFER_RX = 1024, DEFAULT_SERIAL_BUFFER_TX = 512;
    static inline constexpr int DEFAULT_SERIAL_BAUD = 9600;
    static inline constexpr SerialConfig DEFAULT_SERIAL_CONFIG = SERIAL_8N1;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class StreamConnector : public RequestResponseFrame::Receiver {
public:
    explicit StreamConnector (Stream &stream) :
        _stream (stream) {
    }

protected:
    void begin () override {
    }
    void end () override {
        _stream.flush ();
    }
    bool readByte (uint8_t *byte) override {
        if (_stream.available () > 0) {
            *byte = _stream.read ();
            return true;
        }
        return false;
    }
    bool writeBytes (const uint8_t *data, const size_t size) override {
        return _stream.write (data, size) == size;
    }

private:
    Stream &_stream;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms
