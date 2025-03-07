
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifndef DALYBMS_FLATFILES
#pragma once
#endif

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#if ! defined(DALYBMS_DEBUG) && ! defined(DEBUG_START)

#include <Arduino.h>

#ifndef DEBUG_LOGGER_SERIAL_BAUD
#define DEBUG_LOGGER_SERIAL_BAUD 115200
#endif

#ifdef DEBUG
#define DALYBMS_DEBUG_START(...) Serial.begin (DEBUG_LOGGER_SERIAL_BAUD);
#define DALYBMS_DEBUG_END(...) \
    Serial.flush ();   \
    Serial.end ();
#define DALYBMS_DEBUG_PRINTF    Serial.printf
#define ALWAYS_DEBUG_PRINTF    Serial.printf
#else
#define DALYBMS_DEBUG_START(...)
#define DALYBMS_DEBUG_END(...)
#define DALYBMS_DEBUG_PRINTF(...) \
    do {                  \
    } while (0)
#define ALWAYS_DEBUG_PRINTF    Serial.printf        
#endif

#else
#define DALYBMS_DEBUG_START(...)
#define DALYBMS_DEBUG_END(...)
#define DALYBMS_DEBUG_PRINTF(...) \
    do {                  \
    } while (0)
#define ALWAYS_DEBUG_PRINTF    Serial.printf        
#endif

#ifndef PLATFORMIO
#define STATIC_IF_ARDUINO_IDE static
#else
#define STATIC_IF_ARDUINO_IDE
#endif

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

namespace daly_bms {

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <vector>

template <typename T, typename ReturnType = void>
class Handlerable {
public:
    class Handler {
    public:
        using Type = T;
        virtual ~Handler () = default;
        virtual ReturnType handle (T t) = 0;
    };
    void registerHandler (Handler *handler) {
        _handlers.push_back (handler);
    }
    void unregisterHandler (Handler *handler) {
        auto pos = std::find (_handlers.begin (), _handlers.end (), handler);
        if (pos != _handlers.end ())
            _handlers.erase (pos);
    }

private:
    std::vector<Handler *> _handlers;

protected:
    template <typename R = ReturnType>
    typename std::enable_if<std::is_void<R>::value>::type
    notifyHandlers (T t) {
        for (auto handler : _handlers)
            handler->handle (t);
    }
    template <typename R = ReturnType>
    typename std::enable_if<! std::is_void<R>::value, R>::type
    notifyHandlers (T t) {
        R result {};
        for (auto handler : _handlers)
            if ((result = handler->handle (t)))
                break;
        return result;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

template <size_t N>
String BytesToHexString (const uint8_t bytes [], const char *separator = ":") {
    constexpr size_t separator_max = 1;    // change if needed
    if (strlen (separator) > separator_max)
        return String ("");
    char buffer [(N * 2) + ((N - 1) * separator_max) + 1] = { '\0' }, *buffer_ptr = buffer;
    for (size_t i = 0; i < N; i++) {
        if (i > 0 && separator [0] != '\0')
            for (const char *separator_ptr = separator; *separator_ptr != '\0';)
                *buffer_ptr++ = *separator_ptr++;
        static const char hex_chars [] = "0123456789ABCDEF";
        *buffer_ptr++ = hex_chars [(bytes [i] >> 4) & 0x0F];
        *buffer_ptr++ = hex_chars [bytes [i] & 0x0F];
    }
    *buffer_ptr = '\0';
    return String (buffer);
}

// -----------------------------------------------------------------------------------------------

#include <time.h>
#include <stdio.h>    // XXX change from snprintf
#include <Arduino.h>

typedef unsigned long SystemTicks_t;
STATIC_IF_ARDUINO_IDE inline SystemTicks_t systemTicksNow () {
    return millis ();
}
STATIC_IF_ARDUINO_IDE inline unsigned long systemSecsSince (SystemTicks_t ticks) {
    return (millis () - ticks) / 1000;
}

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

template <typename T, T all = T::All>
inline String toStringBitwise (T bits) {
    String x;
    for (T bit = static_cast<T> (1); bit < all; bit = static_cast<T> (static_cast<int> (bit) << 1))
        if ((bits & bit) != T::None)
            x += (x.isEmpty () ? "" : ",") + toString (bit);
    return x;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms
