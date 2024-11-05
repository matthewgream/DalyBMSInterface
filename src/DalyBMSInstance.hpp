
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "DalyBMSUtilities.hpp"
#include "DalyBMSInterface.hpp"
#include "DalyBMSConnector.hpp"
#include "DalyBMSConverterDebug.hpp"
#include "DalyBMSConverterJson.hpp"

#include <memory>

namespace daly_bms {

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

struct Instance {

    struct Config {
        Interface::Config daly;
        int serialBaud{HardwareSerialConnector::DEFAULT_SERIAL_BAUD};
        int serialBufferRx{HardwareSerialConnector::DEFAULT_SERIAL_BUFFER_RX}, serialBufferTx{HardwareSerialConnector::DEFAULT_SERIAL_BUFFER_TX};
        SerialConfig serialConfig{HardwareSerialConnector::DEFAULT_SERIAL_CONFIG};
        int serialId;
        int serialRxPin, serialTxPin;
        int enPin{-1};
    };
    const Config& config;

    using Hardware = HardwareSerial;
    using Connector = HardwareSerialConnector;

    Hardware hardware;
    Connector connector;
    Interface interface;

    Instance(const Config& conf)
        : config(conf), hardware(config.serialId), connector(hardware), interface(config.daly, connector) {
        hardware.setRxBufferSize(config.serialBufferRx);
        hardware.setTxBufferSize(config.serialBufferTx);
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

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class Instances {

public:
    struct Config {
        std::vector<Instance::Config> instances{};
    };

private:
    const Config& config;

    std::vector<std::shared_ptr<Instance>> instances{};
    std::vector<Interface*> interfaces{};

public:
    Instances(const Config& conf)
        : config(conf) {}
    ~Instances() {
        end();
    }

    bool begin() {
        bool began = false;
        exception_catcher(
            [&] {
                for (auto& configInstance : config.instances) {
                    std::shared_ptr <Instance> instance = std::make_shared<Instance>(configInstance);
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
    void debugDump() const {
        for (auto& instance : instances) {
            const auto& config = instance->config;
            DEBUG_PRINTF ("INSTANCE: serial id=%d pin rx=%d, tx=%d, en=%d, config=%08X, bufferRx=%d, bufferTx=%d, baud=%d, config=%d\n",
                config.serialId, config.serialRxPin, config.serialTxPin, config.enPin, config.serialConfig, 
                config.serialBufferRx, config.serialBufferTx, config.serialBaud, config.serialConfig);
           daly_bms::debugDump (instance->interface);
        }
    }
    bool convertToJson(JsonVariant dst) {
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

} // namespace daly_bms
