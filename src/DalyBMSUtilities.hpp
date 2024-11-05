
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#pragma once

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

#define DEBUG
#define DEBUG_LOGGER_SERIAL_BAUD 115200

#ifdef DEBUG
#define DEBUG_START(...) Serial.begin(DEBUG_LOGGER_SERIAL_BAUD);
#define DEBUG_END(...) \
    Serial.flush(); \
    Serial.end();
#define DEBUG_PRINTF Serial.printf
#define DEBUG_ONLY(...) __VA_ARGS__
#else
#define DEBUG_START(...)
#define DEBUG_END(...)
#define DEBUG_PRINTF(...) \
    do { \
    } while (0)
#define DEBUG_ONLY(...)
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
        virtual ~Handler() = default;
        virtual ReturnType handle(T t) = 0;
    };
    void registerHandler(Handler* handler) {
        _handlers.push_back(handler);
    }
    void unregisterHandler(Handler* handler) {
        auto pos = std::find(_handlers.begin(), _handlers.end(), handler);
        if (pos != _handlers.end())
            _handlers.erase(pos);
    }
private:
    std::vector<Handler*> _handlers;
protected:
    template<typename R = ReturnType>
    typename std::enable_if<std::is_void<R>::value>::type
    notifyHandlers(T t) {
        for (auto handler : _handlers)
            handler->handle(t);
    }
    template<typename R = ReturnType>
    typename std::enable_if<!std::is_void<R>::value, R>::type
    notifyHandlers(T t) {
        R result{};
        for (auto handler : _handlers)
            if ((result = handler->handle(t)))
                break;
        return result;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

String toStringHex(const uint8_t data[], const size_t size, const char* separator = "") {
    String result;
    result.reserve(size);
    static const char HEX_CHARS[] = "0123456789ABCDEF";
    for (size_t i = 0; i < size; i++) {
        if (i > 0) result += separator;
        result += HEX_CHARS[data[i] >> 4];
        result += HEX_CHARS[data[i] & 0x0F];
    }
    return result;
}

// -----------------------------------------------------------------------------------------------

#include <time.h>
#include <stdio.h> // XXX change from snprintf
#include <Arduino.h>

typedef unsigned long SystemTicks_t;
SystemTicks_t systemTicksNow () {
    return millis ();
}
unsigned long systemSecsSince(SystemTicks_t ticks) {
    return (millis () - ticks) / 1000;
}

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

template<typename T, T all = T::All>
inline String toStringBitwise(T bits) {
    String x;
    for (T bit = static_cast<T>(1); bit < all; bit = static_cast<T>(static_cast<int>(bit) << 1))
        if ((bits & bit) != T::None)
            x += (x.isEmpty () ? "" : ",") + toString (bit);
    return x;
}

// -----------------------------------------------------------------------------------------------

#include <type_traits>
#include <exception>
#include <utility>

void exception_catcher(
    std::function<void()> f, std::function<void()> g = []() {}) {
    try {
        f();
    } catch (const std::exception& e) {
        DEBUG_PRINTF("exception: %s\n", e.what());
        g();
    } catch (...) {
        DEBUG_PRINTF("exception: unknown\n");
        g();
    }
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

} // namespace daly_bms
