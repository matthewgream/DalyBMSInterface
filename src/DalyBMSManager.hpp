
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "DalyBMSUtilities.hpp"
#include "DalyBMSInterface.hpp"
#include "DalyBMSConvertors.hpp"

#include <memory>

// -----------------------------------------------------------------------------------------------

class DalyAggregationManager {

public:
    static inline constexpr int SERIAL_BUFFER_SIZE = 1024;
    static inline constexpr int SERIAL_BAUD_RATE = 9600;

    using DalyConfiguration = daly_bms::Config;
    using DalyCapabilities = daly_bms::Capabilities;
    using DalyCategories = daly_bms::Categories;

    struct ConfigInstance {
        DalyConfiguration daly;
        int serialBaud{SERIAL_BAUD_RATE};
        int serialBuffer{SERIAL_BUFFER_SIZE};
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
            hardware.begin(config.serialBaud, SERIAL_8N1, config.serialRxPin, config.serialTxPin);
            if (config.enPin >= 0)
                digitalWrite(config.enPin, HIGH);
            interface.begin();
        }
        ~Instance() {
            hardware.flush();
            if (config.enPin >= 0)
                digitalWrite(config.enPin, LOW);
            hardware.end();
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
                    instances.push_back(std::make_shared<Instance>(instanceConfig));
                    interfaces.push_back(&instances.back()->interface);
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

    void loop() {
        forEachInterface<&Interface::loop>();
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
