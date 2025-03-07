
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifndef DALYBMS_FLATFILES
#pragma once
#include "DalyBMSUtilities.hpp"
#include "DalyBMSManager.hpp"
#include "DalyBMSConnector.hpp"
#include "DalyBMSConverterDebug.hpp"
#include "DalyBMSConverterJson.hpp"
#endif

#include <memory>

namespace daly_bms {

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

struct Interface {

    struct Config {
        Manager::Config manager;
        gpio_num_t PIN_EN;
    };
    const Config &config;

    using Connector = StreamConnector;

    Stream &stream;
    Connector connector;
    Manager manager;

    explicit Interface (const Config &c, Stream &s) :
        config (c),
        stream (s),
        connector (stream),
        manager (config.manager, connector) {
        enable (true);
        manager.begin ();
    }
    ~Interface () {
        stream.flush ();
        enable (false);
    }
    void enable (bool enabled) {
        if (config.PIN_EN != GPIO_NUM_NC) {
            pinMode (config.PIN_EN, OUTPUT);
            digitalWrite (config.PIN_EN, enabled ? LOW : HIGH);    // Active-LOW
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class Interfaces {

private:
    const std::vector<Interface::Config> &configs;
    const std::vector<Stream *> streams {};

    std::vector<std::shared_ptr<Interface>> interfaces {};
    std::vector<Manager *> managers {};

public:
    explicit Interfaces (const std::vector<Interface::Config> &c, const std::vector<Stream *> s) :
        configs (c),
        streams (s) { }
    ~Interfaces () {
        end ();
    }

    bool begin () {
        bool began = false;
        try {
            for (int i = 0; i < configs.size (); i++) {
                std::shared_ptr<Interface> interface = std::make_shared<Interface> (configs [i], *streams [i]);
                interfaces.push_back (interface);
                managers.push_back (&interface->manager);
                interface->manager.requestInitial ();
                interface->manager.requestConditions ();
            }
            began = true;
            process ();
        } catch (...) {
            end ();
        }
        return began;
    }

    void end () {
        managers.clear ();
        interfaces.clear ();
    }

    //

    void enable (bool enabled) {
        for (auto &instance : interfaces)
            instance->enable (enabled);
    }
    void process () {
        forEachManager<&Manager::process> ();
    }
    void requestInitial () {
        forEachManager<&Manager::requestInitial> ();
    }
    void updateInitial () {
        forEachManager<&Manager::updateInitial> ();
    }
    void requestInstant () {
        forEachManager<&Manager::requestInstant> ();
    }
    void requestConditions () {
        forEachManager<&Manager::requestConditions> ();
    }
    void requestDiagnostics () {
        forEachManager<&Manager::requestDiagnostics> ();
    }
    void debugDump () const {
        for (const auto &interface : interfaces)
            daly_bms::debugDump (interface->manager);
    }
    bool convertToJson (JsonVariant dst) {
        // XXX TODO
        //     JsonObject serial = dst["serial"].to<JsonObject>();
        //     serial ["id"] = config.serialId;
        //     JsonObject pins = serial["pinx"].to<JsonObject>();
        //     pins ["rx"] = config.serialRxPin;

        //         JsonArray dioStates = dst["dioStates"].to<JsonArray>();
        // for (const auto& state : src.dioStates)
        //     dioStates.add(state);
        // JsonArray i = dst ["instances"].to<JsonObject>;
        // for (auto& instance : instances)
        //     i.add (instance);
        return false;
    }

    struct Status {
        float chargePercentage;
    };
    bool getStatus (Status &s) const {
        for (auto &manager : managers)
            if (manager->getConfig ().id == "manager") {
                const auto &instant_status = manager->conditions.status;
                if (instant_status.isValid ()) {
                    s.chargePercentage = instant_status.charge;
                    return true;
                }
            }
        return false;
    }
    bool setChargingMOSFET (const bool state) {
        for (auto &manager : managers)
            if (manager->getConfig ().id == "manager")
                manager->command (manager->commands.charge, state ? RequestResponse_MOSFET_CHARGE::Setting::On : RequestResponse_MOSFET_CHARGE::Setting::Off);
        return false;
    }

    String status () const {
        String s;
        for (auto &manager : managers) {
            const auto &config = manager->getConfig ();
            const auto &status = manager->getStatus ();

            s += (s.isEmpty () ? "" : ", ") + String ("daly<") + config.id + ">: last=" + String (status.received.seconds ());
            if (config.id == "manager") {
                const auto &instant_status = manager->conditions.status;
                const auto &instant_mosfet = manager->conditions.mosfet;
                const auto &battery_ratings = manager->information.battery_ratings;
                s += ", status=" + instant_status.toString ();
                if (instant_mosfet.isValid ()) {
                    s += ", capacity=" + String (instant_mosfet.residualCapacityAh, 1);
                    if (battery_ratings.isValid ())
                        s += "/" + String (battery_ratings.packCapacityAh, 1);
                    s += "Ah";
                    s += ", state=" + toString (instant_mosfet.state);
                }
                const auto &instant_info = manager->conditions.information;
                if (instant_info.isValid ())
                    s += ", charger=" + String (instant_info.chargerStatus ? "ON" : "OFF") + "/load=" + String (instant_info.loadStatus ? "ON" : "OFF");
            }
            const auto &failures = manager->conditions.failure;
            if (failures.isValid () && failures.count > 0)
                s += ", failures=[" + failures.toString () + "] ";
        }
        return s;
    }

    String info () const {
        String s;
        for (auto &manager : managers) {
            const auto &config = manager->getConfig ();
            double nominalCellVoltage = 0;
            String t;
            if (manager->information.hardware.isValid ())
                t += String (t.isEmpty () ? "" : ", ") + "hardware=" + manager->information.hardware.string;
            if (manager->information.firmware.isValid ())
                t += String (t.isEmpty () ? "" : ", ") + "firmware=" + manager->information.firmware.string;
            if (manager->information.software.isValid ())
                t += String (t.isEmpty () ? "" : ", ") + "software=" + manager->information.software.string;
            if (manager->information.battery_ratings.isValid ()) {
                t += String (t.isEmpty () ? "" : ", ") + "battery=" + String (manager->information.battery_ratings.packCapacityAh, 1) + "Ah/" + String (nominalCellVoltage = manager->information.battery_ratings.nominalCellVoltage, 1) + "V";
                if (manager->information.battery_info.isValid ())
                    t += "/" + toString (manager->information.battery_info.type);
                if (manager->information.config.isValid ()) {
                    int cells = 0;
                    for (const auto &c : manager->information.config.cells)
                        cells += c;
                    t += "/" + String (cells) + "p";
                    if (nominalCellVoltage > 0)
                        t += "/" + String ((double) cells * nominalCellVoltage, 1) + "V";
                }
            }
            s += (s.isEmpty () ? "" : ", ") + String ("daly<") + config.id + ">: " + t;
        }
        return s;
    }

private:
    template <auto MethodPtr>
    void forEachManager () {
        for (auto &manager : managers)
            (manager->*MethodPtr) ();
    }
    template <auto MethodPtr>
    void forEachManager () const {
        for (auto &manager : managers)
            (manager->*MethodPtr) ();
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms
