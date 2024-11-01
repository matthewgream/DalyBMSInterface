
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#define DEBUG
#define DEBUG_LOGGER_SERIAL_BAUD 115200

#ifdef DEBUG
    #define DEBUG_START(...) Serial.begin (DEBUG_LOGGER_SERIAL_BAUD);
    #define DEBUG_END(...) Serial.flush (); Serial.end ();
    #define DEBUG_PRINTF Serial.printf
    #define DEBUG_ONLY(...) __VA_ARGS__
#else
    #define DEBUG_START(...)
    #define DEBUG_END(...)
    #define DEBUG_PRINTF(...) do {} while (0)
    #define DEBUG_ONLY(...)
#endif

// -----------------------------------------------------------------------------------------------

typedef unsigned long interval_t;
typedef unsigned long counter_t;

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class Intervalable {
    interval_t _interval, _previous;

public:
    explicit Intervalable(const interval_t interval = 0, const interval_t previous = 0)
        : _interval(interval), _previous(previous) {}
    operator bool() {
        const interval_t current = millis();
        if (current - _previous > _interval) {
            _previous = current;
            return true;
        }
        return false;
    }
    bool passed(interval_t* interval = nullptr, const bool atstart = false) {
        const interval_t current = millis();
        if ((atstart && _previous == 0) || current - _previous > _interval) {
            if (interval != nullptr)
                (*interval) = current - _previous;
            _previous = current;
            return true;
        }
        return false;
    }
    void reset(const interval_t interval = std::numeric_limits<interval_t>::max()) {
        if (interval != std::numeric_limits<interval_t>::max())
            _interval = interval;
        _previous = millis();
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class ActivationTracker {
    counter_t _count = 0;
    interval_t _seconds = 0;

public:
    inline const interval_t& seconds() const {
        return _seconds;
    }
    inline const counter_t& count() const {
        return _count;
    }
    ActivationTracker& operator++(int) {
        _seconds = millis() / 1000;
        _count++;
        return *this;
    }
    ActivationTracker& operator=(const counter_t count) {
        _seconds = millis() / 1000;
        _count = count;
        return *this;
    }
    inline operator counter_t() const {
        return _count;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class Gate {
    const interval_t _interval;
    interval_t _boundaryLast = 0;
    counter_t _misses = 0;
public:
    explicit Gate(const interval_t interval)
        : _interval(interval) {}
    void waitforThreshold() {
        const interval_t interval = millis() - _boundaryLast;
        if (_interval > interval)
            delay(_interval - interval);
        else if (_boundaryLast > 0) _misses++;
        _boundaryLast = millis();
    }
    counter_t misses() const {
        return _misses;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <type_traits>
#include <exception>
#include <utility>

void exception_catcher(std::function<void()> f, std::function<void()> g = [](){}) {
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
