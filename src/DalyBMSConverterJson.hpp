// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifndef DALYBMS_FLATFILES
#pragma once
#include "DalyBMSUtilities.hpp"
#include "DalyBMSManager.hpp"
#endif

#include <Arduino.h>
#include <ArduinoJson.h>

namespace daly_bms {

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

// -----------------------------------------------------------------------------------------------

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
template<uint8_t COMMAND, typename TYPE, int SIZE, size_t ITEMS_MAX, size_t ITEMS_PER_FRAME, bool FRAMENUM, auto DECODER>
bool convertToJson(const RequestResponse_TYPE_ARRAY<COMMAND, TYPE, SIZE, ITEMS_MAX, ITEMS_PER_FRAME, FRAMENUM, DECODER>& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    // JsonArray values = dst["values"].to<JsonArray>();
    JsonArray values;
    for (const auto& value : src.values)
        values.add(value);
    dst.set(values);
    return true;
}

// -----------------------------------------------------------------------------------------------

bool convertToJson(const RequestResponse_BMS_CONFIG& src, JsonVariant dst) {
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
    dst["operationalMode"] = toString (src.mode);
    dst["type"] = toString (src.type);
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
    dst.set (toString (src.date, src.time));
    return true;
}
bool convertToJson(const RequestResponse_THRESHOLDS_SENSOR& src, JsonVariant dst) {
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
bool convertToJson(const RequestResponse_THRESHOLDS_CELL_SENSOR& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst ["voltageDiff"] = src.voltage;
    dst ["temperatureDiff"] = src.temperature;
    return true;
}
bool convertToJson(const RequestResponse_THRESHOLDS_CELL_BALANCE& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["voltageEnableThreshold"] = src.voltageEnableThreshold;
    dst["voltageAcceptableDifferential"] = src.voltageAcceptableDifference;
    return true;
}
bool convertToJson(const RequestResponse_THRESHOLDS_SHORTCIRCUIT& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["currentShutdownA"] = src.currentShutdownA;
    dst["currentSamplingR"] = src.currentSamplingR;
    return true;
}
bool convertToJson(const RequestResponse_STATUS& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["voltage"] = src.voltage;
    dst["current"] = src.current;
    dst["charge"] = src.charge;
    return true;
}
bool convertToJson(const RequestResponse_MOSFET& src, JsonVariant dst) {
    if (!src.isValid()) return false;
    dst["state"] = toString(src.state);
    dst["mosChargeState"] = src.mosChargeState;
    dst["mosDischargeState"] = src.mosDischargeState;
    dst["bmsLifeCycle"] = src.bmsLifeCycle;
    dst["residualCapacityAh"] = src.residualCapacityAh;
    return true;
}
bool convertToJson(const RequestResponse_INFORMATION& src, JsonVariant dst) {
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
bool convertToJson(const RequestResponse_FAILURE& src, JsonVariant dst) {
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

// -----------------------------------------------------------------------------------------------

template<typename TYPE>
void addToJson(const TYPE flags, JsonArray&& arr) {
    for (TYPE flag = static_cast<TYPE>(1); flag < TYPE::All; flag = static_cast<TYPE>(static_cast<int>(flag) << 1))
        if ((flags & flag) != TYPE::None) arr.add(toString(flag));
}
bool convertToJson(const Manager::Config& src, JsonVariant dst) {
    dst["id"] = src.id;
    addToJson(src.capabilities, dst["capabilities"].to<JsonArray>());
    addToJson(src.categories, dst["categories"].to<JsonArray>());
    addToJson(src.debugging, dst["debugging"].to<JsonArray>());
    return true;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

bool convertToJson(const Manager& src, JsonVariant dst) {

    const auto convertConfig = [&](const Manager::Config& config) {
        dst["config"] = config;
    };
    const auto convertCategory = [&](const Manager::Config& config, const Categories category) -> JsonVariant {
        return dst[toString(category)];
    };
    const auto convertElement = [&](auto&& handler, const auto& component) {
        if (src.isEnabled(&component) && component.isValid())
            handler[getName(component)] = component;
    };

    convertConfig(src.getConfig ());
    if (src.isEnabled(Categories::Information)) {
        auto information = convertCategory(src.getConfig (), Categories::Information);
        convertElement(information, src.information.config);
        convertElement(information, src.information.hardware);
        convertElement(information, src.information.firmware);
        convertElement(information, src.information.software);
        convertElement(information, src.information.battery_ratings);
        convertElement(information, src.information.battery_code);
        convertElement(information, src.information.battery_info);
        convertElement(information, src.information.battery_stat);
        convertElement(information, src.information.rtc);
    }
    if (src.isEnabled(Categories::Thresholds)) {
        auto thresholds = convertCategory(src.getConfig (), Categories::Thresholds);
        convertElement(thresholds, src.thresholds.voltage);
        convertElement(thresholds, src.thresholds.current);
        convertElement(thresholds, src.thresholds.sensor);
        convertElement(thresholds, src.thresholds.charge);
        convertElement(thresholds, src.thresholds.cell_voltage);
        convertElement(thresholds, src.thresholds.cell_sensor);
        convertElement(thresholds, src.thresholds.cell_balance);
        convertElement(thresholds, src.thresholds.shortcircuit);
    }
    if (src.isEnabled(Categories::Status)) {
        auto status = convertCategory(src.getConfig (), Categories::Status);
        convertElement(status, src.status.status);
        convertElement(status, src.status.voltage);
        convertElement(status, src.status.sensor);
        convertElement(status, src.status.mosfet);
        convertElement(status, src.status.information);
        convertElement(status, src.status.failure);
    }
    if (src.isEnabled(Categories::Diagnostics)) {
        auto diagnostics = convertCategory(src.getConfig (), Categories::Diagnostics);
        convertElement(diagnostics, src.diagnostics.voltages);
        convertElement(diagnostics, src.diagnostics.sensors);
        convertElement(diagnostics, src.diagnostics.balances);
    }
    return true;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms
