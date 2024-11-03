// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#pragma once

#include "DalyBMSUtilities.hpp"
#include "DalyBMSInterface.hpp"

#include <Arduino.h> // for String
#include <ArduinoJson.h>

namespace daly_bms {

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <type_traits>

template<typename TYPE> struct TypeName { static constexpr const char* name = "unknown"; };
#define TYPE_NAME(TYPE,NAME) template<> struct TypeName<TYPE> { static constexpr const char* name = NAME; }
TYPE_NAME(RequestResponse_BMS_HARDWARE_CONFIG,"hardware_config");
TYPE_NAME(RequestResponse_BMS_HARDWARE_VERSION,"hardware_version");
TYPE_NAME(RequestResponse_BMS_FIRMWARE_INDEX,"firmware_index");
TYPE_NAME(RequestResponse_BMS_SOFTWARE_VERSION,"software_version");
TYPE_NAME(RequestResponse_BATTERY_RATINGS,"battery_ratings");
TYPE_NAME(RequestResponse_BATTERY_CODE,"battery_code");
TYPE_NAME(RequestResponse_BATTERY_INFO,"battery_info");
TYPE_NAME(RequestResponse_BATTERY_STAT,"battery_stat");
TYPE_NAME(RequestResponse_BMS_RTC,"rtc");
TYPE_NAME(RequestResponse_PACK_VOLTAGES_THRESHOLDS,"pack_voltages");
TYPE_NAME(RequestResponse_PACK_CURRENTS_THRESHOLDS,"pack_currents");
TYPE_NAME(RequestResponse_PACK_TEMPERATURE_THRESHOLDS,"pack_temperatures");
TYPE_NAME(RequestResponse_PACK_SOC_THRESHOLDS,"pack_soc");
TYPE_NAME(RequestResponse_CELL_VOLTAGES_THRESHOLDS,"cell_voltages");
TYPE_NAME(RequestResponse_CELL_SENSORS_THRESHOLDS,"cell_sensors");
TYPE_NAME(RequestResponse_CELL_BALANCES_THRESHOLDS,"cell_balances");
TYPE_NAME(RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS,"pack_shortcircuit");
TYPE_NAME(RequestResponse_PACK_STATUS,"pack");
TYPE_NAME(RequestResponse_CELL_VOLTAGES_MINMAX,"cell_voltages");
TYPE_NAME(RequestResponse_CELL_TEMPERATURES_MINMAX,"cell_temperatures");
TYPE_NAME(RequestResponse_MOSFET_STATUS,"fets");
TYPE_NAME(RequestResponse_PACK_INFORMATION,"info");
TYPE_NAME(RequestResponse_FAILURE_STATUS,"failures");
TYPE_NAME(RequestResponse_CELL_VOLTAGES,"voltages");
TYPE_NAME(RequestResponse_CELL_TEMPERATURES,"temperatures");
TYPE_NAME(RequestResponse_CELL_BALANCES,"balances");
template<typename TYPE> constexpr const char* getName(const TYPE&) {
    return TypeName<TYPE>::name;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<typename TYPE>
bool convertToJson(const FrameTypeMinmax<TYPE>& src, JsonVariant dst) {
    dst["max"] = src.max;
    dst["min"] = src.min;
    return true;
}
template<typename TYPE>
bool convertToJson(const FrameTypeThresholdsMinmax<TYPE>& src, JsonVariant dst) {
    dst["L1"] = src.L1;
    dst["L2"] = src.L2;
    return true;
}
template<typename TYPE>
bool convertToJson(const FrameTypeThresholdsDifference<TYPE>& src, JsonVariant dst) {
    dst["L1"] = src.L1;
    dst["L2"] = src.L2;
    return true;
}

template<uint8_t COMMAND, int LENGTH>
bool convertToJson(const RequestResponse_TYPE_STRING<COMMAND, LENGTH>& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst.set(src.string);
    return true;
}
template<uint8_t COMMAND, typename TYPE, int SIZE, auto DECODER>
bool convertToJson(const RequestResponse_TYPE_THRESHOLD_MINMAX<COMMAND, TYPE, SIZE, DECODER>& src, JsonVariant dst) {
    if (!src.isValid()) return false;
  //  convertToJson (src.value, dst);
    dst.set (src.value);
    // dst["maxL1"] = src.maxL1;
    // dst["maxL2"] = src.maxL2;
    // dst["minL1"] = src.minL1;
    // dst["minL2"] = src.minL2;
    return true;
}
template<uint8_t COMMAND, typename TYPE, int SIZE, auto DECODER>
bool convertToJson(const RequestResponse_TYPE_VALUE_MINMAX<COMMAND, TYPE, SIZE, DECODER>& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    JsonObject max = dst["max"].to<JsonObject>();
    max["value"] = src.value.max;
    max["cell"] = src.cellNumber.max;
    JsonObject min = dst["min"].to<JsonObject>();
    min["value"] = src.value.min;
    min["cell"] = src.cellNumber.min;
    return true;
}
template<uint8_t COMMAND, typename TYPE, int SIZE, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, auto DECODER>
bool convertToJson(const RequestResponse_TYPE_ARRAY<COMMAND, TYPE, SIZE, ITEMS_MAX, ITEMS_PER_FRAME, DECODER>& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    // JsonArray values = dst["values"].to<JsonArray>();
    JsonArray values;
    for (const auto& value : src.values)
        values.add(value);
    dst.set(values);
    return true;
}
//
bool convertToJson(const RequestResponse_BMS_HARDWARE_CONFIG& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["boards"] = src.boards;
    JsonArray cells = dst["cells"].to<JsonArray>();
    JsonArray sensors = dst["sensors"].to<JsonArray>();
    for (int i = 0; i < 3; i++)
        cells.add(src.cells[i]), sensors.add(src.sensors[i]);
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
    dst["operationalMode"] = src.operationalMode;
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
    dst ["charge"] = src.charge;
    dst ["discharge"] = src.discharge;
    // JsonObject charge = dst["charge"].to<JsonObject>();
    // charge["maxL1"] = src.chargeMaxL1;
    // charge["maxL2"] = src.chargeMaxL2;
    // charge["minL1"] = src.chargeMinL1;
    // charge["minL2"] = src.chargeMinL2;
    // JsonObject discharge = dst["discharge"].to<JsonObject>();
    // discharge["maxL1"] = src.dischargeMaxL1;
    // discharge["maxL2"] = src.dischargeMaxL2;
    // discharge["minL1"] = src.dischargeMinL1;
    // discharge["minL2"] = src.dischargeMinL2;
    return true;
}
bool convertToJson(const RequestResponse_CELL_SENSORS_THRESHOLDS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst ["voltageDiff"] = src.voltage;
    dst ["temperatureDiff"] = src.temperature;
    return true;
}
bool convertToJson(const RequestResponse_CELL_BALANCES_THRESHOLDS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["voltageEnableThreshold"] = src.voltageEnableThreshold;
    dst["voltageAcceptableDifferential"] = src.voltageAcceptableDifference;
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
    dst["charge"] = src.charge;
    return true;
}
bool convertToJson(const RequestResponse_MOSFET_STATUS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["state"] = toString(src.state);
    dst["mosChargeState"] = src.mosChargeState;
    dst["mosDischargeState"] = src.mosDischargeState;
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
template<typename TYPE>
void addToJson(const TYPE flags, JsonArray&& arr) {
    for (TYPE flag = static_cast<TYPE>(1); flag < TYPE::All; flag = static_cast<TYPE>(static_cast<int>(flag) << 1))
        if ((flags & flag) != TYPE::None) arr.add(toString(flag));
}
bool convertToJson(const Config& src, JsonVariant dst) {
    dst["id"] = src.id;
    addToJson(src.capabilities, dst["capabilities"].to<JsonArray>());
    addToJson(src.categories, dst["categories"].to<JsonArray>());
    return true;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

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

template<typename TYPE>
void debugDump(const TYPE&) {
    DEBUG_PRINTF("<unknown>\n");
}
template<uint8_t COMMAND, int LENGTH>
void debugDump(const RequestResponse_TYPE_STRING<COMMAND, LENGTH>& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("%s\n", src.string.c_str());
}
template<uint8_t COMMAND, typename TYPE, int SIZE, auto DECODER>
void debugDump(const RequestResponse_TYPE_THRESHOLD_MINMAX<COMMAND, TYPE, SIZE, DECODER>& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("max L1=%s,L2=%s, min L1=%s,L2=%s\n",    // XXX units
                 detail::toString(src.value.L1.max).c_str(),
                 detail::toString(src.value.L2.max).c_str(),
                 detail::toString(src.value.L1.min).c_str(),
                 detail::toString(src.value.L2.min).c_str());
}
template<uint8_t COMMAND, typename TYPE, int SIZE, auto DECODER>
void debugDump(const RequestResponse_TYPE_VALUE_MINMAX<COMMAND, TYPE, SIZE, DECODER>& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("max=%s (#%d), min=%s (#%d)\n",    // XXX units
                 detail::toString(src.value.max).c_str(),
                 src.cellNumber.max,
                 detail::toString(src.value.min).c_str(),
                 src.cellNumber.min);
}

template<uint8_t COMMAND, typename TYPE, int SIZE, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, auto DECODER>
void debugDump(const RequestResponse_TYPE_ARRAY<COMMAND, TYPE, SIZE, ITEMS_MAX, ITEMS_PER_FRAME, DECODER>& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("%u /", src.values.size());
    for (const auto& v : src.values)
        DEBUG_PRINTF(" %s", detail::toString(v).c_str());    // XXX units
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
    DEBUG_PRINTF("packCapacity=%.1fAh, nominalCellVoltage=%.1fV\n",
                 src.packCapacityAh, src.nominalCellVoltage);
}
void debugDump(const RequestResponse_BATTERY_STAT& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("cumulativeCharge=%.1fAh, cumulativeDischarge=%.1fAh\n",
                 src.cumulativeChargeAh, src.cumulativeDischargeAh);
}
void debugDump(const RequestResponse_BATTERY_INFO& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("operationalMode=%d, type=%d, productionDate=%s, sleep=%d, unknown 1=%d, 2=%d\n",
                 src.operationalMode, src.type,
                 src.productionDate.toString().c_str(),
                 src.automaticSleepSec,
                 src.unknown1, src.unknown2);
}
void debugDump(const RequestResponse_PACK_TEMPERATURE_THRESHOLDS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("charge max L1=%dC,L2=%dC min L1=%dC,L2=%dC, discharge max L1=%dC,L2=%dC min L1=%dC,L2=%dC\n",
                 src.charge.L1.max, src.charge.L2.max,
                 src.charge.L1.min, src.charge.L2.min,
                 src.discharge.L1.max, src.discharge.L2.max,
                 src.discharge.L1.min, src.discharge.L2.min);
}
void debugDump(const RequestResponse_CELL_SENSORS_THRESHOLDS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("voltage diff L1=%.3fV,L2=%.3fV, temperature diff L1=%dC,L2=%dC\n",
                 src.voltage.L1, src.voltage.L2,
                 src.temperature.L1, src.temperature.L2);
}
void debugDump(const RequestResponse_CELL_BALANCES_THRESHOLDS& src) {
    if (!src.isValid()) return;
    DEBUG_PRINTF("voltage enable=%.3fV, acceptable=%.3fV\n",
                 src.voltageEnableThreshold,
                 src.voltageAcceptableDifference);
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
                 src.voltage, src.current, src.charge);
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
    DEBUG_PRINTF("show=%s, count=%d", src.show ? "yes" : "no", src.count);
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

    const auto convertConfig = [&](const Config& config) {
        dst["config"] = config;
    };
    const auto convertCategory = [&](const Config& config, const Categories category) -> JsonVariant {
        return dst[toString(category)];
    };
    const auto convertElement = [&](auto&& handler, const auto& component) {
        if (src.isEnabled(&component) && component.isValid())
            handler[getName(component)] = component;
    };

    convertConfig(src.getConfig ());
    if (src.isEnabled(Categories::Information)) {
        auto information = convertCategory(src.getConfig (), Categories::Information);
        convertElement(information, src.information.hardware_config);
        convertElement(information, src.information.hardware_version);
        convertElement(information, src.information.firmware_index);
        convertElement(information, src.information.software_version);
        convertElement(information, src.information.battery_ratings);
        convertElement(information, src.information.battery_code);
        convertElement(information, src.information.battery_info);
        convertElement(information, src.information.battery_stat);
        convertElement(information, src.information.rtc);
    }
    if (src.isEnabled(Categories::Thresholds)) {
        auto thresholds = convertCategory(src.getConfig (), Categories::Thresholds);
        convertElement(thresholds, src.thresholds.pack_voltages);
        convertElement(thresholds, src.thresholds.pack_currents);
        convertElement(thresholds, src.thresholds.pack_temperatures);
        convertElement(thresholds, src.thresholds.pack_soc);
        convertElement(thresholds, src.thresholds.cell_voltages);
        convertElement(thresholds, src.thresholds.cell_sensors);
        convertElement(thresholds, src.thresholds.cell_balances);
        convertElement(thresholds, src.thresholds.pack_shortcircuit);
    }
    if (src.isEnabled(Categories::Status)) {
        auto status = convertCategory(src.getConfig (), Categories::Status);
        convertElement(status, src.status.pack);
        convertElement(status, src.status.cell_voltages);
        convertElement(status, src.status.cell_temperatures);
        convertElement(status, src.status.fets);
        convertElement(status, src.status.info);
        convertElement(status, src.status.failures);
    }
    if (src.isEnabled(Categories::Diagnostics)) {
        auto diagnostics = convertCategory(src.getConfig (), Categories::Diagnostics);
        convertElement(diagnostics, src.diagnostics.voltages);
        convertElement(diagnostics, src.diagnostics.temperatures);
        convertElement(diagnostics, src.diagnostics.balances);
    }
    return true;
}

void dumpDebug(const Interface& src) {

    const auto convertConfig = [&](const Config& config) {
        DEBUG_PRINTF("DalyBMS<%s>: capabilities=%s, categories=%s\n", config.id, toStringBitwise(config.capabilities), toStringBitwise(config.categories));
    };
    const auto convertCategory = [&](const Config& config, const Categories category) -> String {
        DEBUG_PRINTF("DalyBMS<%s>: %s:\n", config.id, toString(category));
        return toString(category);
    };
    const auto convertElement = [&](auto&&, const auto& component) {
        if (!src.isEnabled(&component)) return false;
        if (component.isValid()) {
            DEBUG_PRINTF("  %s: <%s> ", getName(component), toStringISO (component.valid ()));
            debugDump(component);
            return true;
        } else { 
            DEBUG_PRINTF("  %s: <Not valid>", getName(component));
            return false;
        }
    };

    convertConfig(src.getConfig ());
    if (src.isEnabled(Categories::Information)) {
        auto information = convertCategory(src.getConfig (), Categories::Information);
        convertElement(information, src.information.hardware_config);
        convertElement(information, src.information.hardware_version);
        convertElement(information, src.information.firmware_index);
        convertElement(information, src.information.software_version);
        convertElement(information, src.information.battery_ratings);
        convertElement(information, src.information.battery_code);
        convertElement(information, src.information.battery_info);
        convertElement(information, src.information.battery_stat);
        convertElement(information, src.information.rtc);
    }
    if (src.isEnabled(Categories::Thresholds)) {
        auto thresholds = convertCategory(src.getConfig (), Categories::Thresholds);
        convertElement(thresholds, src.thresholds.pack_voltages);
        convertElement(thresholds, src.thresholds.pack_currents);
        convertElement(thresholds, src.thresholds.pack_temperatures);
        convertElement(thresholds, src.thresholds.pack_soc);
        convertElement(thresholds, src.thresholds.cell_voltages);
        convertElement(thresholds, src.thresholds.cell_sensors);
        convertElement(thresholds, src.thresholds.cell_balances);
        convertElement(thresholds, src.thresholds.pack_shortcircuit);
    }
    if (src.isEnabled(Categories::Status)) {
        auto status = convertCategory(src.getConfig (), Categories::Status);
        convertElement(status, src.status.pack);
        convertElement(status, src.status.cell_voltages);
        convertElement(status, src.status.cell_temperatures);
        convertElement(status, src.status.fets);
        convertElement(status, src.status.info);
        convertElement(status, src.status.failures);
    }
    if (src.isEnabled(Categories::Diagnostics)) {
        auto diagnostics = convertCategory(src.getConfig (), Categories::Diagnostics);
        convertElement(diagnostics, src.diagnostics.voltages);
        convertElement(diagnostics, src.diagnostics.temperatures);
        convertElement(diagnostics, src.diagnostics.balances);
    }
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms
