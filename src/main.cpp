
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

#define DEBUG
#define DEBUG_LOGGER_SERIAL_BAUD 115200

#ifdef DEBUG
    #define DEBUG_START(...) Serial.begin (DEBUG_LOGGER_SERIAL_BAUD);
    #define DEBUG_END(...) Serial.flush (); Serial.end ();
    #define DEBUG_PRINTF Serial.printf
    #define DEBUG_ONLY(...) __VA_ARGS__
#else
    #define DEBUG_START(...)
    #define DEBUG_END(...)
    #define DEBUG_PRINTF(...) do {} while (0)
    #define DEBUG_ONLY(...)
#endif

// -----------------------------------------------------------------------------------------------

#include "Utilities.hpp"
#include "ComponentsHardwareDalyBMS.hpp"

// -----------------------------------------------------------------------------------------------

#include <memory>

class DalyAggregationManager {

public:
    static inline constexpr int SERIAL_BUFFER_SIZE = 1024;
    static inline constexpr int SERIAL_BAUD_RATE = 9600;
    
    using DalyConfiguration = daly_bms::Interface::Config;
    using DalyCapabilities = daly_bms::Capabilities;
    using DalyCategories = daly_bms::Categories;

    struct ConfigInstance {
        DalyConfiguration daly;
        int serialBaud = SERIAL_BAUD_RATE;
        int serialBuffer = SERIAL_BUFFER_SIZE;
        int serialId;
        int serialRxPin, serialTxPin;
        int enPin = -1;
    };
    struct Config {
        std::vector<ConfigInstance> instances;
    };

private:
    const Config& config;
    
    using Hardware = HardwareSerial;
    using Connector = daly_bms::HardwareSerialConnector;
    using Interface = daly_bms::Interface;

    struct Instance {
        const ConfigInstance &config;
        Hardware hardware;
        Connector connector;
        Interface interface;
        Instance(const ConfigInstance& conf): config (conf), hardware (config.serialId), connector (hardware), interface (config.daly, connector) {
            hardware.setRxBufferSize (config.serialBuffer);
            hardware.begin (config.serialBaud, SERIAL_8N1, config.serialRxPin, config.serialTxPin);    
            if (config.enPin >= 0)
                digitalWrite (config.enPin, HIGH);
            interface.begin ();
        }
        ~Instance() {
            hardware.flush ();
            if (config.enPin >= 0)
                digitalWrite (config.enPin, LOW);
            hardware.end ();                       
        }
    };

    std::vector<std::shared_ptr<Instance>> instances;
    std::vector<Interface*> interfaces;

public:
    DalyAggregationManager (const Config &conf): config (conf) {}
    ~DalyAggregationManager () { end (); }

    bool begin () {
        bool began = false;
        exception_catcher(
            [&] {
                for (auto& instanceConfig: config.instances) {
                    instances.push_back (std::make_shared<Instance> (instanceConfig));
                    interfaces.push_back (&instances.back ()->interface);
                }
                began = true;
            },
            [&] { end (); });
        return began;
    }

    void end () {
        interfaces.clear ();
        instances.clear ();
    }

    //

    void loop() { forEachInterface<&Interface::loop>(); }
    void requestStatus() { forEachInterface<&Interface::requestStatus>(); }
    void requestDiagnostics() { forEachInterface<&Interface::requestDiagnostics>(); }
    void dumpDebug() const { forEachInterface<&Interface::dumpDebug>(); }

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

void testDirect () {

    const int serialId = 1, serialRxPin = 5, serialTxPin = 4, enPin = 5;
    HardwareSerial* serial;
    
    exception_catcher([&] {
        serial = new HardwareSerial(serialId);
        serial->setRxBufferSize(1024);
        serial->begin(9600, SERIAL_8N1, serialRxPin, serialTxPin);
        if (enPin >= 0) digitalWrite (enPin, HIGH);

        while (1) {
            // uint8_t command [13] = { 0xA5, 0x40, 0xD9, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06 };
            //uint8_t command [13] = { 0xA5, 0x40 , 0x90 , 0x08 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x7D };
            uint8_t command[13] = { 0xa5, 0x40, 0x62, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f };
            serial->write(command, 13);
            while (serial->available() > 0) {
                uint8_t byte = serial->read();
                DEBUG_PRINTF("<%02X>", byte);
            }
            DEBUG_PRINTF("*\n");
            delay(5000);
        }
    });
}

// -----------------------------------------------------------------------------------------------

Gate processGate(5 * 1000);
Intervalable requestStatus(15 * 1000), requestDiagnostics(30 * 1000), reportData(10 * 1000);

DalyAggregationManager* dalyManager = nullptr;

void setup() {

    delay(5 * 1000);
    DEBUG_START();
    DEBUG_PRINTF("*** SETUP\n");

    //testDirect ();

    DalyAggregationManager::Config *config = new DalyAggregationManager::Config ({
        .instances = {  
            {    
                .daly = {
                    .id = "manager",
                    .capabilities = DalyAggregationManager::DalyCapabilities::All,
                    .categories = DalyAggregationManager::DalyCategories::All
                },
                .serialId = 1,
                .serialRxPin = 0,
                .serialTxPin = 1,
                .enPin = 2
            },
            {    
                .daly = {
                    .id = "balancer",
                    .capabilities = DalyAggregationManager::DalyCapabilities::All,
                    .categories = DalyAggregationManager::DalyCategories::All
                },
                .serialId = 2,
                .serialRxPin = 3,
                .serialTxPin = 4,
                .enPin = 5
            },            
        }
    });
    dalyManager = new DalyAggregationManager(*config);
    if (!dalyManager || !dalyManager->begin ()) {
        DEBUG_PRINTF("*** FAILED\n");
        esp_deep_sleep_start();
    }
}

void loop() {
    processGate.waitforThreshold();
    exception_catcher([&] {
        DEBUG_PRINTF("*** LOOP\n");
        dalyManager->loop();
        if (requestStatus) dalyManager->requestStatus();
        if (requestDiagnostics) dalyManager->requestDiagnostics();
        if (reportData) dalyManager->dumpDebug();
    });
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
