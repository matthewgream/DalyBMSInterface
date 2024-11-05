// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifndef DALYBMS_FLATFILES
#pragma once
#include "DalyBMSRequestResponse.hpp"
#endif

#include <cstdint>
#include <bitset>
#include <array>
#include <vector>

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

namespace daly_bms {

namespace detail {
    inline String toString(float v) {
        return String(v, 3);
    }
    template<typename TYPE>
    static typename std::enable_if<
        std::disjunction<
            std::is_same<TYPE, int8_t>,
            std::is_same<TYPE, uint8_t>
        >::value,
        String>::type
    toString(TYPE v) {
        return String(static_cast<int>(v));
    }
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

/*
  offical 0x90-0x98
  https://robu.in/wp-content/uploads/2021/10/Daly-CAN-Communications-Protocol-V1.0-1.pdf

  unofficial
  https://diysolarforum.com/threads/decoding-the-daly-smartbms-protocol.21898/
  https://diysolarforum.com/threads/daly-bms-communication-protocol.65439/
*/

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

struct FrameTypeDateYMD {
    uint8_t year{}, month{}, day{};  // 0-99 + 2000
    String toString() const {
        return String("20") + (year < 10 ? "0" : "") + String(year) + (month < 10 ? "0" : "") + String(month) + (day < 10 ? "0" : "") + String(day);
    }
};
struct FrameTypeDateHMS {
    uint8_t hours{}, minutes{}, seconds{};
    String toString() const {
        return (hours < 10 ? "0" : "") + String(hours) + (minutes < 10 ? "0" : "") + String(minutes) + (seconds < 10 ? "0" : "") + String(seconds);
    }
};
String toString (const FrameTypeDateYMD& date, const FrameTypeDateHMS& time) {
    return String("20") + (date.year < 10 ? "0" : "") + String(date.year) + (date.month < 10 ? "/0" : "/") + String(date.month) + (date.day < 10 ? "/0" : "/") + String(date.day) +
        String ('T') +
        (time.hours < 10 ? "0" : "") + String(time.hours) + (time.minutes < 10 ? ":0" : ":") + String(time.minutes) + (time.seconds < 10 ? ":0" : ":") + String(time.seconds);
}
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

// -----------------------------------------------------------------------------------------------

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
        *value = frame.getBit(offset);
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
    static bool decode(const RequestResponseFrame& frame, size_t offset, FrameTypeDateHMS* value) {
        *value = { .hours = frame.getUInt8(offset + 0), .minutes = frame.getUInt8(offset + 1), .seconds = frame.getUInt8(offset + 2) };
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

// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND>
class RequestResponseCommand : public RequestResponse {
public:
    RequestResponseCommand()
        : RequestResponse(RequestResponse::Builder().setCommand(COMMAND)) {}
    static constexpr const char* getTypeName() { return "command"; }
    const char* getName() const override { return getTypeName (); }
    void debugDump() const override {}
protected:
    using RequestResponse::isValid;
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

// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND, int LENGTH = 1>
class RequestResponse_TYPE_STRING : public RequestResponseCommand<COMMAND> {
public:
    String string;
    RequestResponse_TYPE_STRING(): RequestResponseCommand<COMMAND>() {
        setResponseFrameCount(LENGTH);
    }
    static constexpr const char* getTypeName() { return "RequestResponse_TYPE_STRING"; }
    const char* getName() const override { return getTypeName (); }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("%s\n", string.c_str());
    }
    using RequestResponseCommand<COMMAND>::isValid;
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

class RequestResponse_BATTERY_RATINGS : public RequestResponseCommand<0x50> {
public:
    double packCapacityAh{};
    double nominalCellVoltage{};
    const char* getName() const override { return "RequestResponse_BATTERY_RATINGS"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("packCapacity=%.1fAh, nominalCellVoltage=%.1fV\n",
                    packCapacityAh, nominalCellVoltage);
    }
    using RequestResponseCommand<0x50>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &packCapacityAh, 1000.0) && FrameContentDecoder::decode(frame, 4, &nominalCellVoltage, 1000.0));
    }
};

// -----------------------------------------------------------------------------------------------

class RequestResponse_BMS_CONFIG : public RequestResponseCommand<0x51> {
public:
    uint8_t boards{};
    std::array<uint8_t, 3> cells{};
    std::array<uint8_t, 3> sensors{};
    const char* getName() const override { return "RequestResponse_BMS_CONFIG"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("boards=%d, cells=%d,%d,%d, sensors=%d,%d,%d\n",
                    boards,
                    cells[0], cells[1], cells[2],
                    sensors[0], sensors[1], sensors[2]);
    }
    using RequestResponseCommand<0x51>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &boards) && FrameContentDecoder::decode<3>(frame, 1, &cells) && FrameContentDecoder::decode<3>(frame, 4, &sensors));
    }
};

// -----------------------------------------------------------------------------------------------

class RequestResponse_BATTERY_STAT : public RequestResponseCommand<0x52> {    // XXX TBC
public:
    double cumulativeChargeAh{}; // XXX should be custom type "Cumulative"
    double cumulativeDischargeAh{};
    const char* getName() const override { return "RequestResponse_BATTERY_STAT"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("cumulativeCharge=%.1fAh, cumulativeDischarge=%.1fAh\n",
                        cumulativeChargeAh, cumulativeDischargeAh);
    }
    using RequestResponseCommand<0x52>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &cumulativeChargeAh) && FrameContentDecoder::decode(frame, 4, &cumulativeDischargeAh));
    }
};

// -----------------------------------------------------------------------------------------------

enum class OperationalMode : uint8_t { LongPressPowerOnOff = 0x01 };
String toString(const OperationalMode operationalMode) {
    switch (operationalMode) {
        case OperationalMode::LongPressPowerOnOff: return "long-press power-on/off";
        default: return String ("0x") + String (static_cast <uint8_t> (operationalMode), HEX);
    }
}
enum class BatteryType : uint8_t { LithiumIon = 0x01 };
String toString(const BatteryType batteryType) {
    switch (batteryType) {
        case BatteryType::LithiumIon: return "lithium-ion";
        default: return String ("0x") + String (static_cast <uint8_t> (batteryType), HEX);
    }
}
class RequestResponse_BATTERY_INFO : public RequestResponseCommand<0x53> {    // XXX TBC
public:
    OperationalMode mode{};
    BatteryType type{};
    FrameTypeDateYMD productionDate{};
    uint16_t automaticSleepSec{};
    uint8_t unknown1{}, unknown2{};
    const char* getName() const override { return "RequestResponse_BATTERY_INFO"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("mode=%s, type=%s, date=%s, sleep=%d, unknown 1=%d, 2=%d\n",
                    toString (mode).c_str (), toString (type).c_str (),
                    productionDate.toString().c_str(),
                    automaticSleepSec,
                    unknown1, unknown2);
    }
    using RequestResponseCommand<0x53>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &mode) && FrameContentDecoder::decode(frame, 1, &type) && FrameContentDecoder::decode(frame, 2, &productionDate) && FrameContentDecoder::decode_Time_s(frame, 5, &automaticSleepSec) && FrameContentDecoder::decode(frame, 6, &unknown1) && FrameContentDecoder::decode(frame, 7, &unknown2));
    }
};

// -----------------------------------------------------------------------------------------------

using RequestResponse_BMS_FIRMWARE = RequestResponse_TYPE_STRING<0x54, 1>;
template<>constexpr const char* RequestResponse_TYPE_STRING<0x54, 1>::getTypeName() { return "RequestResponse_BMS_FIRMWARE"; }

// -----------------------------------------------------------------------------------------------

using RequestResponse_BATTERY_CODE = RequestResponse_TYPE_STRING<0x57, 5>;
template<>constexpr const char* RequestResponse_TYPE_STRING<0x57, 5>::getTypeName() { return "RequestResponse_BATTERY_CODE"; }

// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND, typename TYPE, int SIZE, auto DECODER>
class RequestResponse_TYPE_THRESHOLD_MINMAX : public RequestResponseCommand<COMMAND> {
public:
    FrameTypeThresholdsMinmax<TYPE> value{};
    static constexpr const char* getTypeName() { return "RequestResponse_TYPE_THRESHOLD_MINMAX"; }
    const char* getName() const override { return getTypeName (); }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("max L1=%s,L2=%s, min L1=%s,L2=%s\n",    // XXX units
                    detail::toString(value.L1.max).c_str(),
                    detail::toString(value.L2.max).c_str(),
                    detail::toString(value.L1.min).c_str(),
                    detail::toString(value.L2.min).c_str());
    }
    using RequestResponseCommand<COMMAND>::isValid;
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid( // XXX should be template
            DECODER(frame, SIZE * 0, &value.L1.max) && DECODER(frame, SIZE * 1, &value.L2.max) && DECODER(frame, SIZE * 2, &value.L1.min) && DECODER(frame, SIZE * 3, &value.L2.min));
    }
};

// -----------------------------------------------------------------------------------------------

using RequestResponse_THRESHOLDS_CELL_VOLTAGE = RequestResponse_TYPE_THRESHOLD_MINMAX<0x59, float, 2, FrameContentDecoder::decode_Voltage_m>;
template<>constexpr const char* RequestResponse_TYPE_THRESHOLD_MINMAX<0x59, float, 2, FrameContentDecoder::decode_Voltage_m>::getTypeName() { return "RequestResponse_THRESHOLDS_CELL_VOLTAGE"; };

// -----------------------------------------------------------------------------------------------

using RequestResponse_THRESHOLDS_VOLTAGE = RequestResponse_TYPE_THRESHOLD_MINMAX<0x5A, float, 2, FrameContentDecoder::decode_Voltage_d>;
template<>constexpr const char* RequestResponse_TYPE_THRESHOLD_MINMAX<0x5A, float, 2, FrameContentDecoder::decode_Voltage_d>::getTypeName() { return "RequestResponse_THRESHOLDS_VOLTAGE"; };

// -----------------------------------------------------------------------------------------------

using RequestResponse_THRESHOLDS_CURRENT = RequestResponse_TYPE_THRESHOLD_MINMAX<0x5B, float, 2, FrameContentDecoder::decode_Current_d>;
template<>constexpr const char* RequestResponse_TYPE_THRESHOLD_MINMAX<0x5B, float, 2, FrameContentDecoder::decode_Current_d>::getTypeName() { return "RequestResponse_THRESHOLDS_CURRENT"; };

// -----------------------------------------------------------------------------------------------

class RequestResponse_THRESHOLDS_SENSOR : public RequestResponseCommand<0x5C> {
public:
    FrameTypeThresholdsMinmax<int8_t> charge{}, discharge{};
    const char* getName() const override { return "RequestResponse_THRESHOLDS_SENSOR"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("charge max L1=%dC,L2=%dC min L1=%dC,L2=%dC, discharge max L1=%dC,L2=%dC min L1=%dC,L2=%dC\n",
                    charge.L1.max, charge.L2.max,
                    charge.L1.min, charge.L2.min,
                    discharge.L1.max, discharge.L2.max,
                    discharge.L1.min, discharge.L2.min);

    }
    using RequestResponseCommand<0x5C>::isValid;
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

// -----------------------------------------------------------------------------------------------

using RequestResponse_THRESHOLDS_CHARGE = RequestResponse_TYPE_THRESHOLD_MINMAX<0x5D, float, 2, FrameContentDecoder::decode_Percent_d>;
template<> constexpr const char* RequestResponse_TYPE_THRESHOLD_MINMAX<0x5D, float, 2, FrameContentDecoder::decode_Percent_d>::getTypeName() { return "RequestResponse_THRESHOLDS_CHARGE"; };

// -----------------------------------------------------------------------------------------------

class RequestResponse_THRESHOLDS_CELL_SENSOR : public RequestResponseCommand<0x5E> {
public:
    FrameTypeThresholdsDifference<float> voltage{};
    FrameTypeThresholdsDifference<int8_t> temperature{};
    const char* getName() const override { return "RequestResponse_THRESHOLDS_CELL_SENSOR"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("voltage diff L1=%.3fV,L2=%.3fV, temperature diff L1=%dC,L2=%dC\n",
                    voltage.L1, voltage.L2,
                    temperature.L1, temperature.L2);
    }
    using RequestResponseCommand<0x5E>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(  // XXX should be template
            FrameContentDecoder::decode_Voltage_m(frame, 0, &voltage.L1) && FrameContentDecoder::decode_Voltage_m(frame, 2, &voltage.L2) &&
            FrameContentDecoder::decode_Temperature(frame, 4, &temperature.L1) && FrameContentDecoder::decode_Temperature(frame, 5, &temperature.L2));
    }
};

// -----------------------------------------------------------------------------------------------

class RequestResponse_THRESHOLDS_CELL_BALANCE : public RequestResponseCommand<0x5F> {
public:
    float voltageEnableThreshold{};
    float voltageAcceptableDifference{};
    const char* getName() const override { return "RequestResponse_THRESHOLDS_CELL_BALANCE"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("voltage enable=%.3fV, acceptable=%.3fV\n",
                    voltageEnableThreshold,
                    voltageAcceptableDifference);
    }
    using RequestResponseCommand<0x5F>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Voltage_m(frame, 0, &voltageEnableThreshold) && FrameContentDecoder::decode_Voltage_m(frame, 2, &voltageAcceptableDifference));
    }
};

// -----------------------------------------------------------------------------------------------

class RequestResponse_THRESHOLDS_SHORTCIRCUIT : public RequestResponseCommand<0x60> {
public:
    float currentShutdownA{};
    float currentSamplingR{};
    const char* getName() const override { return "RequestResponse_THRESHOLDS_SHORTCIRCUIT"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("shutdown=%.1fA, sampling=%.3fR\n",
                 currentShutdownA, currentSamplingR);
    }
    using RequestResponseCommand<0x60>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &currentShutdownA) && FrameContentDecoder::decode(frame, 2, &currentSamplingR, 1000.0f));
    }
};

// -----------------------------------------------------------------------------------------------

class RequestResponse_BMS_RTC : public RequestResponseCommand<0x61> {
public:
    FrameTypeDateYMD date{};
    FrameTypeDateHMS time{};
    const char* getName() const override { return "RequestResponse_BMS_RTC"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("%s\n", toString (date, time).c_str ());
    }
    using RequestResponseCommand<0x61>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &date) && FrameContentDecoder::decode(frame, 3, &time));
    }
};

// -----------------------------------------------------------------------------------------------

using RequestResponse_BMS_SOFTWARE = RequestResponse_TYPE_STRING<0x62, 2>;
template<> constexpr const char* RequestResponse_TYPE_STRING<0x62, 2>::getTypeName() { return "RequestResponse_BMS_SOFTWARE"; };

// -----------------------------------------------------------------------------------------------

using RequestResponse_BMS_HARDWARE = RequestResponse_TYPE_STRING<0x63, 2>;
template<> constexpr const char* RequestResponse_TYPE_STRING<0x63, 2>::getTypeName() { return "RequestResponse_BMS_HARDWARE"; };

// -----------------------------------------------------------------------------------------------

class RequestResponse_STATUS : public RequestResponseCommand<0x90> {
public:
    float voltage{};
    float current{};
    float charge{};
    const char* getName() const override { return "RequestResponse_STATUS"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("%.1f volts, %.1f amps, %.1f percent\n",
                 voltage, current, charge);
    }
    using RequestResponseCommand<0x90>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode_Voltage_d(frame, 0, &voltage) &&
            FrameContentDecoder::decode_Current_d(frame, 4, &current) &&
            FrameContentDecoder::decode_Percent_d(frame, 6, &charge));
    }
};

// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND, typename TYPE, int SIZE, auto DECODER>
class RequestResponse_TYPE_VALUE_MINMAX : public RequestResponseCommand<COMMAND> {
public:
    FrameTypeMinmax<TYPE> value{};
    FrameTypeMinmax<uint8_t> cellNumber{};
    static constexpr const char* getTypeName() { return "RequestResponse_TYPE_VALUE_MINMAX"; }
    const char* getName() const override { return getTypeName (); }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("max=%s (#%d), min=%s (#%d)\n",    // XXX units
                    detail::toString(value.max).c_str(),
                    cellNumber.max,
                    detail::toString(value.min).c_str(),
                    cellNumber.min);
    }
    using RequestResponseCommand<COMMAND>::isValid;
    using RequestResponseCommand<COMMAND>::setValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid( // XXX should be template
            DECODER(frame, 0, &value.max) &&
            FrameContentDecoder::decode(frame, SIZE, &cellNumber.max) &&
            DECODER(frame, SIZE + 1, &value.min) &&
            FrameContentDecoder::decode(frame, SIZE + 1 + SIZE, &cellNumber.min));
    }
};

// -----------------------------------------------------------------------------------------------

using RequestResponse_VOLTAGE_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x91, float, 2, FrameContentDecoder::decode_Voltage_m>;
template<>constexpr const char* RequestResponse_TYPE_VALUE_MINMAX<0x91, float, 2, FrameContentDecoder::decode_Voltage_m>::getTypeName () { return "RequestResponse_VOLTAGE_MINMAX"; };

// -----------------------------------------------------------------------------------------------

using RequestResponse_SENSOR_MINMAX = RequestResponse_TYPE_VALUE_MINMAX<0x92, int8_t, 1, FrameContentDecoder::decode_Temperature>;
template<>constexpr const char* RequestResponse_TYPE_VALUE_MINMAX<0x92, int8_t, 1, FrameContentDecoder::decode_Temperature>::getTypeName () { return "RequestResponse_SENSOR_MINMAX"; };

// -----------------------------------------------------------------------------------------------

enum class ChargeState : uint8_t { Stationary = 0x00, Charge = 0x01, Discharge = 0x02 };
String toString(const ChargeState state) {
    switch (state) {
        case ChargeState::Stationary: return "stationary";
        case ChargeState::Charge: return "charge";
        case ChargeState::Discharge: return "discharge";
        default: return String ("0x") + String (static_cast <uint8_t> (state), HEX);
    }
}

class RequestResponse_MOSFET : public RequestResponseCommand<0x93> {
public:
    ChargeState state{};
    bool mosChargeState{};
    bool mosDischargeState{};
    uint8_t bmsLifeCycle{};
    double residualCapacityAh{};
    const char* getName() const override { return "RequestResponse_MOSFET"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("state=%s, MOS charge=%s, discharge=%s, cycle=%d, capacity=%.1fAh\n",
                toString(state).c_str(),
                mosChargeState ? "on" : "off",
                mosDischargeState ? "on" : "off",
                bmsLifeCycle,
                residualCapacityAh);
    }
    using RequestResponseCommand<0x93>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &state) && FrameContentDecoder::decode(frame, 1, &mosChargeState) && FrameContentDecoder::decode(frame, 2, &mosDischargeState) && FrameContentDecoder::decode(frame, 3, &bmsLifeCycle) && FrameContentDecoder::decode(frame, 4, &residualCapacityAh, 1000.0));
    }
};

// -----------------------------------------------------------------------------------------------

class RequestResponse_INFORMATION : public RequestResponseCommand<0x94> {
public:
    uint8_t numberOfCells{};
    uint8_t numberOfSensors{};
    bool chargerStatus{};
    bool loadStatus{};
    std::array<bool, 8> dioStates{};
    uint16_t cycles{};
    const char* getName() const override { return "RequestResponse_INFORMATION"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("cells=%d, sensors=%d, charger=%s, load=%s, cycles=%d\n",
                 numberOfCells, numberOfSensors,
                 chargerStatus ? "on" : "off",
                 loadStatus ? "on" : "off",
                 cycles);
    }
    using RequestResponseCommand<0x94>::isValid;
protected:
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t) override {
        return setValid(
            FrameContentDecoder::decode(frame, 0, &numberOfCells) && FrameContentDecoder::decode(frame, 1, &numberOfSensors) && FrameContentDecoder::decode(frame, 2, &chargerStatus) && FrameContentDecoder::decode(frame, 3, &loadStatus) && FrameContentDecoder::decode(frame, 4, &dioStates) && FrameContentDecoder::decode(frame, 5, &cycles));
    }
};

// -----------------------------------------------------------------------------------------------

template<uint8_t COMMAND, typename TYPE, int SIZE, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, bool FRAMENUM, auto DECODER>
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
    static constexpr const char* getTypeName() { return "RequestResponse_TYPE_ARRAY"; }
    const char* getName() const override { return getTypeName (); }
    void debugDump () const override {
        if (!isValid()) return;
        DEBUG_PRINTF("%u /", values.size());
        for (const auto& v : values)
            DEBUG_PRINTF(" %s", detail::toString(v).c_str());    // XXX units
        DEBUG_PRINTF("\n");
    }
    using RequestResponseCommand<COMMAND>::isValid;
protected:
    using RequestResponseCommand<COMMAND>::setValid;
    using RequestResponseCommand<COMMAND>::setResponseFrameCount;
    bool processResponseFrame(const RequestResponseFrame& frame, const size_t frameNum) override {
        if (FRAMENUM && (frame.getUInt8(0) != frameNum || frame.getUInt8(0) > (values.size() / ITEMS_PER_FRAME) + 1)) return false;
        for (size_t i = 0; i < ITEMS_PER_FRAME && (((frameNum - 1) * ITEMS_PER_FRAME) + i) < values.size(); i++)
            if (!DECODER(frame, (FRAMENUM ? 1 : 0) + i * SIZE, &values[((frameNum - 1) * ITEMS_PER_FRAME) + i]))
                return setValid (false); // will block remaining frames
        if (frameNum == (values.size() / ITEMS_PER_FRAME) + 1)
            return setValid();
        return true;
    }
};

// -----------------------------------------------------------------------------------------------

using RequestResponse_VOLTAGES = RequestResponse_TYPE_ARRAY<0x95, float, 2, 48, 3, true, FrameContentDecoder::decode_Voltage_m>;
template<> constexpr const char* RequestResponse_TYPE_ARRAY<0x95, float, 2, 48, 3, true, FrameContentDecoder::decode_Voltage_m>::getTypeName() { return "RequestResponse_VOLTAGES"; };

// -----------------------------------------------------------------------------------------------

using RequestResponse_SENSORS = RequestResponse_TYPE_ARRAY<0x96, int8_t, 1, 16, 7, true, FrameContentDecoder::decode_Temperature>;
template<> constexpr const char* RequestResponse_TYPE_ARRAY<0x96, int8_t, 1, 16, 7, true, FrameContentDecoder::decode_Temperature>::getTypeName() { return "RequestResponse_SENSORS"; };

// -----------------------------------------------------------------------------------------------

using RequestResponse_BALANCES = RequestResponse_TYPE_ARRAY<0x97, uint8_t, 1, 48, 48, false, FrameContentDecoder::decode_BitNoFrameNum>;
template<> constexpr const char* RequestResponse_TYPE_ARRAY<0x97, uint8_t, 1, 48, 48, false, FrameContentDecoder::decode_BitNoFrameNum>::getTypeName() { return "RequestResponse_BALANCES"; };

// -----------------------------------------------------------------------------------------------

class RequestResponse_FAILURE : public RequestResponseCommand<0x98> {
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
    const char* getName() const override { return "RequestResponse_FAILURE"; }
    void debugDump() const override {
        if (!isValid()) return;
        DEBUG_PRINTF("show=%s, count=%d", show ? "yes" : "no", count);
        if (count > 0) {
            DEBUG_PRINTF(", active=[");
            const char* failures[count];
            for (size_t i = 0, c = getFailureList(failures, count); i < c; i++)
                DEBUG_PRINTF("%s%s", i == 0 ? "" : ",", failures[i]);
            DEBUG_PRINTF("]");
        }
        DEBUG_PRINTF("\n");
    }
    using RequestResponseCommand<0x98>::isValid;
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

template<uint8_t COMMAND>
class RequestResponse_TYPE_ONOFF : public RequestResponseCommand<COMMAND> {
public:
    enum class Setting : uint8_t { Off = 0x00,
                                   On = 0x01 };
    RequestResponseFrame prepareRequest(const Setting setting) {
        return RequestResponse::prepareRequest().setUInt8(4, static_cast<uint8_t>(setting)).finalize();
    }
    static constexpr const char* getTypeName() { return "RequestResponse_TYPE_ONOFF"; }
    const char* getName() const override { return getTypeName (); }
};

// -----------------------------------------------------------------------------------------------

using RequestResponse_RESET = RequestResponseCommand<0x00>;
template<> constexpr const char* RequestResponseCommand<0x00>::getTypeName() { return "RequestResponse_RESET"; }

// -----------------------------------------------------------------------------------------------

using RequestResponse_MOSFET_DISCHARGE = RequestResponse_TYPE_ONOFF<0xD9>;
template<> constexpr const char* RequestResponse_TYPE_ONOFF<0xD9>::getTypeName() { return "RequestResponse_MOSFET_DISCHARGE"; }

// -----------------------------------------------------------------------------------------------

using RequestResponse_MOSFET_CHARGE = RequestResponse_TYPE_ONOFF<0xDA>;
template<> constexpr const char* RequestResponse_TYPE_ONOFF<0xDA>::getTypeName() { return "RequestResponse_MOSFET_CHARGE"; }

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <type_traits>

template<typename TYPE> struct TypeName { static constexpr const char* name = "unknown"; };
#define TYPE_NAME(TYPE,NAME) template<> struct TypeName<TYPE> { static constexpr const char* name = NAME; }
TYPE_NAME(RequestResponse_BMS_CONFIG,"config");
TYPE_NAME(RequestResponse_BMS_HARDWARE,"hardware");
TYPE_NAME(RequestResponse_BMS_FIRMWARE,"firmware");
TYPE_NAME(RequestResponse_BMS_SOFTWARE,"software");
TYPE_NAME(RequestResponse_BATTERY_RATINGS,"battery_ratings");
TYPE_NAME(RequestResponse_BATTERY_CODE,"battery_code");
TYPE_NAME(RequestResponse_BATTERY_INFO,"battery_info");
TYPE_NAME(RequestResponse_BATTERY_STAT,"battery_stat");
TYPE_NAME(RequestResponse_BMS_RTC,"rtc");
TYPE_NAME(RequestResponse_THRESHOLDS_VOLTAGE,"voltage");
TYPE_NAME(RequestResponse_THRESHOLDS_CURRENT,"current");
TYPE_NAME(RequestResponse_THRESHOLDS_SENSOR,"sensor");
TYPE_NAME(RequestResponse_THRESHOLDS_CHARGE,"charge");
TYPE_NAME(RequestResponse_THRESHOLDS_SHORTCIRCUIT,"shortcircuit");
TYPE_NAME(RequestResponse_THRESHOLDS_CELL_VOLTAGE,"cell_voltage");
TYPE_NAME(RequestResponse_THRESHOLDS_CELL_SENSOR,"cell_sensor");
TYPE_NAME(RequestResponse_THRESHOLDS_CELL_BALANCE,"cell_balance");
TYPE_NAME(RequestResponse_STATUS,"status");
TYPE_NAME(RequestResponse_VOLTAGE_MINMAX,"voltage");
TYPE_NAME(RequestResponse_SENSOR_MINMAX,"sensor");
TYPE_NAME(RequestResponse_MOSFET,"mosfet");
TYPE_NAME(RequestResponse_INFORMATION,"info");
TYPE_NAME(RequestResponse_FAILURE,"failure");
TYPE_NAME(RequestResponse_VOLTAGES,"voltages");
TYPE_NAME(RequestResponse_SENSORS,"sensors");
TYPE_NAME(RequestResponse_BALANCES,"balances");
template<typename TYPE> constexpr const char* getName(const TYPE&) {
    return TypeName<TYPE>::name;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

} // namespace daly_bms

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
