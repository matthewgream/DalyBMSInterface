
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
    static inline constexpr const char *TYPE_MANAGER = "manager";
    static inline constexpr const char *TYPE_BALANCE = "balance";

    struct Config {
        Manager::Config manager;
        gpio_num_t PIN_EN;
    };
    const Config &config;

    using Connector = StreamConnector;

    Connector connector;
    Manager manager;
    Enableable started;

    explicit Interface (const Config &c, Stream &s) :
        config (c),
        connector (s),
        manager (config.manager, connector) {
    }
    void _enable (bool enabled) {
        if (config.PIN_EN != GPIO_NUM_NC) {
            pinMode (config.PIN_EN, OUTPUT);
            digitalWrite (config.PIN_EN, enabled ? LOW : HIGH);    // Active-LOW
        }
    }
    void begin () {
        _enable (true);
        manager.begin ();
        if (! started) {
            manager.requestInitial ();
            manager.requestConditions ();
            started++;
        }
    }
    void end () {
        manager.end ();
        _enable (false);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class Interfaces {

private:
    using Configs = std::vector<Interface::Config>;
    using Streams = std::vector<Stream *>;
    using Interfacez = std::vector<std::shared_ptr<Interface>>;
    using Managers = std::vector<Manager *>;

    const Configs &configs;
    const Interfacez interfaces;
    const Managers managers;

    static Interfacez makeInterfaces (const Configs &configs, const Streams &streams) {
        Interfacez result;
        result.reserve (configs.size ());
        for (size_t i = 0; i < configs.size (); i++)
            result.push_back (std::make_shared<Interface> (configs [i], *streams [i]));
        return result;
    }
    static Managers makeManagers (const Interfacez &interfaces) {
        Managers result;
        result.reserve (interfaces.size ());
        for (const auto &interface : interfaces)
            result.push_back (&interface->manager);
        return result;
    }

public:
    explicit Interfaces (const Configs &c, const Streams s) :
        configs (c),
        interfaces (makeInterfaces (c, s)),
        managers (makeManagers (interfaces)) {
    }

    bool begin () {
        for (const auto &interface : interfaces)
            interface->begin ();
        process ();
        return true;
    }
    void end () {
        process ();
        for (const auto &interface : interfaces)
            interface->end ();
    }

    //

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

    static constexpr bool is_manager (const Manager *manager) {
        return manager->getConfig ().id == Interface::TYPE_MANAGER;
    }

    struct Status {
        interval_t timestamp = 0;
        float chargePercentage = 0.0f;
        enum class MosState {
            Unspecified,
            On,
            Off
        } mosCharge = MosState::Unspecified,
          mosDischarge = MosState::Unspecified;
        int failureCount = -1;
        String failureList;
    };
    bool getStatus (Status &s) const {
        bool result = false;
        for (auto &manager : managers) {
            if (is_manager (manager)) {
                const auto &instant_status = manager->conditions.status;
                if (instant_status.isValid ()) {
                    s.timestamp = instant_status.valid ();
                    s.chargePercentage = instant_status.charge;
                    result = true;
                }
                const auto &instant_mosfet = manager->conditions.mosfet;
                if (instant_mosfet.isValid ()) {
                    s.mosCharge = instant_mosfet.mosChargeState ? Status::MosState::On : Status::MosState::Off;
                    s.mosDischarge = instant_mosfet.mosDischargeState ? Status::MosState::On : Status::MosState::Off;
                }
            }
            const auto &instant_failure = manager->conditions.failure;
            if (instant_failure.isValid () && instant_failure.count > 0) {
                s.failureCount = (s.failureCount == -1 ? 0 : s.failureCount) + instant_failure.count;
                const String failureString = instant_failure.toString ();
                s.failureList += (! s.failureList.isEmpty () && ! failureString.isEmpty () ? "," : "") + failureString;
            }
        }
        return result;
    }
    const daly_bms::Manager::Conditions *getConditions () const {
        for (auto &manager : managers)
            if (is_manager (manager))
                return &manager->conditions;
        return nullptr;
    }
    const daly_bms::Manager::Diagnostics *getDiagnostics () const {
        for (auto &manager : managers)
            if (is_manager (manager))
                return &manager->diagnostics;
        return nullptr;
    }

    bool setChargeMOSFET (const bool state) {
        for (auto &manager : managers)
            if (is_manager (manager))
                manager->command (manager->commands.charge, state ? RequestResponse_MOSFET_CHARGE::Setting::On : RequestResponse_MOSFET_CHARGE::Setting::Off);
        return false;
    }
    bool setDischargeMOSFET (const bool state) {
        for (auto &manager : managers)
            if (is_manager (manager))
                manager->command (manager->commands.discharge, state ? RequestResponse_MOSFET_DISCHARGE::Setting::On : RequestResponse_MOSFET_DISCHARGE::Setting::Off);
        return false;
    }

    interval_t received () const {
        interval_t r = millis () / 1000;
        for (auto &manager : managers)
            r = std::min (r, manager->getStatus ().received.seconds ());
        return r;
    }
    counter_t badframes () const {
        counter_t c = 0;
        for (auto &manager : managers)
            c += manager->getStatus ().badframes.count ();
        return c;
    }
    String status () const {
        String s;
        for (auto &manager : managers) {
            const auto &config = manager->getConfig ();
            const auto &status = manager->getStatus ();

            s += (s.isEmpty () ? "" : ", ") + String ("daly<") + config.id + ">: last=" + String (status.received.seconds ());
            if (is_manager (manager)) {
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
