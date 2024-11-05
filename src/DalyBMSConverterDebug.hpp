// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#pragma once

#include "DalyBMSUtilities.hpp"
#include "DalyBMSInterface.hpp"

#include <Arduino.h> // String

namespace daly_bms {

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

void debugDump(const Interface& src) {

    const auto convertConfig = [&](const Interface::Config& config) {
        DEBUG_PRINTF("DalyBMS<%s>: capabilities=%s, categories=%s, debugging=%s\n", 
        config.id, toStringBitwise(config.capabilities).c_str (), toStringBitwise(config.categories).c_str (), toStringBitwise(config.debugging).c_str ());
    };
    const auto convertCategory = [&](const Interface::Config& config, const Categories category) -> String {
        DEBUG_PRINTF("DalyBMS<%s>: %s:\n", config.id, toString(category).c_str ());
        return toString(category);
    };
    const auto convertElement = [&](auto&&, const auto& component) {
        if (!src.isEnabled(&component)) return false;
        if (component.isValid()) {
            DEBUG_PRINTF("  %s: <%lu> ", getName(component), systemSecsSince (component.valid ()));
            component.debugDump();
            return true;
        } else { 
            DEBUG_PRINTF("  %s: <Not valid>\n", getName(component));
            return false;
        }
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
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms
