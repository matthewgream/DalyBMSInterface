
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "DalyBMSUtilities.hpp"
#include "DalyBMSInterface.hpp"
#include "DalyBMSConvertors.hpp"

#include <memory>

// -----------------------------------------------------------------------------------------------

class DalyAggregationManager {

public:
    static inline constexpr int DEFAULT_SERIAL_BUFFER = 1024;
    static inline constexpr int DEFAULT_SERIAL_BAUD = 9600;
    static inline constexpr SerialConfig DEFAULT_SERIAL_CONFIG = SERIAL_8N1;

    using DalyConfiguration = daly_bms::Config;
    using DalyCapabilities = daly_bms::Capabilities;
    using DalyCategories = daly_bms::Categories;
    using DalyDebugging = daly_bms::Debugging;

    struct ConfigInstance {
        DalyConfiguration daly;
        int serialBaud{DEFAULT_SERIAL_BAUD};
        int serialBuffer{DEFAULT_SERIAL_BUFFER};
        SerialConfig serialConfig{DEFAULT_SERIAL_CONFIG};
        int serialId;
        int serialRxPin, serialTxPin;
        int enPin{-1};
    };
    struct Config {
        std::vector<ConfigInstance> instances{};
    };

private:
    const Config& config;

    using Hardware = HardwareSerial;
    using Connector = daly_bms::HardwareSerialConnector;
    using Interface = daly_bms::Interface;

    struct Instance {
        const ConfigInstance& config;
        Hardware hardware;
        Connector connector;
        Interface interface;
        Instance(const ConfigInstance& conf)
            : config(conf), hardware(config.serialId), connector(hardware), interface(config.daly, connector) {
            hardware.setRxBufferSize(config.serialBuffer);
            hardware.begin(config.serialBaud, config.serialConfig, config.serialRxPin, config.serialTxPin);
            enable (true);
            interface.begin();
        }
        ~Instance() {
            hardware.flush();
            enable (false);
            hardware.end();
        }
        void enable (bool enabled) {
            if (config.enPin >= 0) digitalWrite(config.enPin, enabled ? LOW : HIGH); // Active-LOW
        }
    };

    std::vector<std::shared_ptr<Instance>> instances{};
    std::vector<Interface*> interfaces{};

public:
    DalyAggregationManager(const Config& conf)
        : config(conf) {}
    ~DalyAggregationManager() {
        end();
    }

    bool begin() {
        bool began = false;
        daly_bms::exception_catcher(
            [&] {
                for (auto& instanceConfig : config.instances) {
                    std::shared_ptr <Instance> instance = std::make_shared<Instance>(instanceConfig);
                    instances.push_back(instance);
                    interfaces.push_back(&instance->interface);
                    instance->interface.requestInitial ();
                    instance->interface.requestStatus ();
                }
                began = true;
            },
            [&] {
                end();
            });
        return began;
    }

    void end() {
        interfaces.clear();
        instances.clear();
    }

    //

    void enable(bool enabled) {
        for (auto& instance : instances) instance->enable (enabled);
    }
    void process() {
        forEachInterface<&Interface::process>();
    }
    void requestInitial() {
        forEachInterface<&Interface::requestInitial>();
    }
    void updateInitial() {
        forEachInterface<&Interface::updateInitial>();
    }
    void requestStatus() {
        forEachInterface<&Interface::requestStatus>();
    }
    void requestDiagnostics() {
        forEachInterface<&Interface::requestDiagnostics>();
    }
    void dumpDebug() const {
        forEachInterface<&Interface::dumpDebug>();
    }

private:
    template<auto MethodPtr> void forEachInterface() {
        for (auto& interface : interfaces)
            (interface->*MethodPtr)();
    }
    template<auto MethodPtr> void forEachInterface() const {
        for (auto& interface : interfaces)
            (interface->*MethodPtr)();
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
