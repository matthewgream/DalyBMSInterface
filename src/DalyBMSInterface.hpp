
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
        int serialBaud { HardwareSerialConnector::DEFAULT_SERIAL_BAUD };
        int serialBufferRx { HardwareSerialConnector::DEFAULT_SERIAL_BUFFER_RX }, serialBufferTx { HardwareSerialConnector::DEFAULT_SERIAL_BUFFER_TX };
        SerialConfig serialConfig { HardwareSerialConnector::DEFAULT_SERIAL_CONFIG };
        int serialId;
        int serialRxPin, serialTxPin;
        int enPin { -1 };
    };
    const Config &config;

    using Hardware = HardwareSerial;
    using Connector = HardwareSerialConnector;

    Hardware hardware;
    Connector connector;
    Manager manager;

    explicit Interface (const Config &conf) :
        config (conf),
        hardware (config.serialId),
        connector (hardware),
        manager (config.manager, connector) {
        hardware.setRxBufferSize (config.serialBufferRx);
        hardware.setTxBufferSize (config.serialBufferTx);
        hardware.begin (config.serialBaud, config.serialConfig, config.serialRxPin, config.serialTxPin);
        enable (true);
        manager.begin ();
    }
    ~Interface () {
        hardware.flush ();
        enable (false);
        hardware.end ();
    }
    void enable (bool enabled) {
        if (config.enPin >= 0)
            digitalWrite (config.enPin, enabled ? LOW : HIGH);    // Active-LOW
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class Interfaces {

public:
    struct Config {
        std::vector<Interface::Config> interfaces {};
    };

private:
    const Config &config;

    std::vector<std::shared_ptr<Interface>> interfaces {};
    std::vector<Manager *> managers {};

public:
    explicit Interfaces (const Config &conf) :
        config (conf) { }
    ~Interfaces () {
        end ();
    }

    bool begin () {
        bool began = false;
        try {
            for (auto &configInterface : config.interfaces) {
                std::shared_ptr<Interface> interface = std::make_shared<Interface> (configInterface);
                interfaces.push_back (interface);
                managers.push_back (&interface->manager);
                interface->manager.requestInitial ();
                interface->manager.requestStatus ();
            }
            began = true;
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
    void requestStatus () {
        forEachManager<&Manager::requestStatus> ();
    }
    void requestDiagnostics () {
        forEachManager<&Manager::requestDiagnostics> ();
    }
    void debugDump () const {
        for (const auto &interface : interfaces) {
            const auto &config = interface->config;
            DEBUG_PRINTF ("INTERFACE: serial id=%d pin rx=%d, tx=%d, en=%d, config=%08X, bufferRx=%d, bufferTx=%d, baud=%d, config=%d\n",
                          config.serialId,
                          config.serialRxPin,
                          config.serialTxPin,
                          config.enPin,
                          config.serialConfig,
                          config.serialBufferRx,
                          config.serialBufferTx,
                          config.serialBaud,
                          config.serialConfig);
            daly_bms::debugDump (interface->manager);
        }
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
