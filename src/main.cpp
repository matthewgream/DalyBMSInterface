
#if defined(DALYBMS_STANDALONE) || ! defined(PLATFORMIO)

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

typedef unsigned long interval_t;
class Intervalable {
    interval_t _interval, _previous {};

public:
    explicit Intervalable (const interval_t interval = 0) :
        _interval (interval) { }
    operator bool () {
        const interval_t current = millis ();
        if (current - _previous > _interval) {
            _previous = current;
            return true;
        }
        return false;
    }
    void wait () {
        const interval_t current = millis ();
        if (current - _previous < _interval)
            delay (_interval - (current - _previous));
        _previous = millis ();
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifdef DALYBMS_FLATFILES
#include "src/DalyBMSUtilities.hpp"
#include "src/DalyBMSRequestResponse.hpp"
#include "src/DalyBMSRequestResponseTypes.hpp"
#include "src/DalyBMSManager.hpp"
#include "src/DalyBMSConnector.hpp"
#include "src/DalyBMSConverterDebug.hpp"
#include "src/DalyBMSConverterJson.hpp"
#include "src/DalyBMSInterface.hpp"
#else
#include "DalyBMSInterface.hpp"
#endif

// -----------------------------------------------------------------------------------------------

// TEST_DEVICE = ESP32-S3-DEVKITC-1
// https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html
// device 1 - GPIO 5, 6, 7 (rx, tx, en) + gnd
// device 2 - GPIO 15, 16, 17 (rx, tx, en) + gnd
static constexpr gpio_num_t BALANCE_PIN_RX = GPIO_NUM_6, BALANCE_PIN_TX = GPIO_NUM_5, BALANCE_PIN_EN = GPIO_NUM_7;
static constexpr int BALANCE_ID = 2;
static constexpr const char *BALANCE_NAME = "balance";
static constexpr gpio_num_t MANAGER_PIN_RX = GPIO_NUM_16, MANAGER_PIN_TX = GPIO_NUM_15, MANAGER_PIN_EN = GPIO_NUM_17;
static constexpr int MANAGER_ID = 1;
static constexpr const char *MANAGER_NAME = "manager";

/*

blue, ground
green, 3.3v
white, 12v
yellow, s1
black, tx (bms)
red, rx (bms)

*/

// -----------------------------------------------------------------------------------------------

void testRaw () {

    const int serialId = MANAGER_ID;
    const gpio_num_t serialRxPin = MANAGER_PIN_RX, serialTxPin = MANAGER_PIN_TX, enPin = MANAGER_PIN_EN;
    // const int serialId = BALANCE_ID;
    // const gpio_num_t serialRxPin = BALANCE_PIN_RX, serialTxPin = BALANCE_PIN_TX, enPin = BALANCE_PIN_EN;

    try {
        HardwareSerial serial (serialId);
        serial.begin (daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_BAUD, daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_CONFIG, serialRxPin, serialTxPin);
        if (enPin >= 0)
            digitalWrite (enPin, LOW);

        while (1) {
            const uint8_t command [13] = { 0xA5, 0x40, 0x90, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7D };
            serial.write (command, 13);
            while (serial.available () > 0) {
                const uint8_t byte = serial.read ();
                DEBUG_PRINTF ("<%02X>", byte);
            }
            DEBUG_PRINTF ("*\n");
            delay (5000);
        }
    } catch (const std::exception &e) {
        DEBUG_PRINTF ("exception: %s\n", e.what ());
    } catch (...) {
        DEBUG_PRINTF ("exception: unknown\n");
    }
}

// -----------------------------------------------------------------------------------------------

void testOne () {

    const int serialId = MANAGER_ID, serialRxPin = MANAGER_PIN_RX, serialTxPin = MANAGER_PIN_TX, enPin = MANAGER_PIN_EN;
    daly_bms::Manager::Config config = {
        .id = "manager",
        .capabilities = daly_bms::Capabilities::All,
        .categories = daly_bms::Categories::All,
        .debugging = daly_bms::Debugging::All
    };
    // const int serialId = BALANCE_ID, serialRxPin = BALANCE_PIN_RX, serialTxPin = BALANCE_PIN_TX, enPin = BALANCE_PIN_EN;
    // daly_bms::Manager::Config config = {
    //         .id = "balance",
    //         .capabilities = daly_bms::Capabilities::All,
    //         .categories = daly_bms::Categories::All,
    //         .debugging = daly_bms::Debugging::All
    // };

    HardwareSerial serial (serialId);
    serial.begin (daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_BAUD, daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_CONFIG, serialRxPin, serialTxPin);
    if (enPin >= 0)
        digitalWrite (enPin, LOW);
    daly_bms::HardwareSerialConnector connector (serial);
    daly_bms::Manager manager (config, connector);
    manager.begin ();

    while (1) {
        auto &request = manager.information.config;
        // manager.issue (manager.status.info); // required to capture numbers for diagnostics request/responses
        manager.issue (request);
        delay (1000);
        manager.process ();
        if (request.isValid ()) {
            DEBUG_PRINTF ("---> \n");
            request.debugDump ();
            DEBUG_PRINTF ("<--- \n");
        }
        DEBUG_PRINTF (".\n");
        delay (4000);
    }
}

void testTwo () {

    const int serialIdA = MANAGER_ID, serialRxPinA = MANAGER_PIN_RX, serialTxPinA = MANAGER_PIN_TX, enPinA = MANAGER_PIN_EN;
    daly_bms::Manager::Config configA = {
        .id = "manager",
        .capabilities = daly_bms::Capabilities::All,
        .categories = daly_bms::Categories::All,
        .debugging = daly_bms::Debugging::All
    };
    const int serialIdB = BALANCE_ID, serialRxPinB = BALANCE_PIN_RX, serialTxPinB = BALANCE_PIN_TX, enPinB = BALANCE_PIN_EN;
    daly_bms::Manager::Config configB = {
        .id = "balance",
        .capabilities = daly_bms::Capabilities::All,
        .categories = daly_bms::Categories::All,
        .debugging = daly_bms::Debugging::All
    };

    HardwareSerial serialA (serialIdA);
    HardwareSerial serialB (serialIdB);
    // pinMode (serialRxPinA, INPUT);
    // pinMode (serialRxPinB, INPUT);
    // pinMode (serialTxPinA, OUTPUT);
    // pinMode (serialTxPinB, OUTPUT);
    serialA.begin (daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_BAUD, daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_CONFIG, serialRxPinA, serialTxPinA);
    serialB.begin (daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_BAUD, daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_CONFIG, serialRxPinB, serialTxPinB);
    if (enPinA >= 0)
        pinMode (enPinA, OUTPUT), digitalWrite (enPinA, LOW);
    if (enPinB >= 0)
        pinMode (enPinB, OUTPUT), digitalWrite (enPinB, LOW);
    daly_bms::HardwareSerialConnector connectorA (serialA);
    daly_bms::HardwareSerialConnector connectorB (serialB);
    daly_bms::Manager managerA (configA, connectorA);
    daly_bms::Manager managerB (configB, connectorB);
    managerA.begin ();
    managerB.begin ();

    while (1) {
        const interval_t now = millis ();
        Serial.printf ("TIME = %f mins\n", ((float)now) / 1000.0f/60.0f);
        auto &requestA = managerA.information.config;
        managerA.issue (requestA);
        auto &requestB = managerB.information.config;
        managerB.issue (requestB);
        delay (1000);
        managerA.process ();
        managerB.process ();
        DEBUG_PRINTF ("---> \n");
        if (requestA.isValid ())
            requestA.debugDump ();
        if (requestB.isValid ())
            requestB.debugDump ();
        DEBUG_PRINTF ("<--- \n");

        DEBUG_PRINTF (".\n");
        delay (4000);
    }
}

// -----------------------------------------------------------------------------------------------

Intervalable processInterval (5 * 1000), requestStatus (15 * 1000), requestDiagnostics (30 * 1000), reportData (30 * 1000);

daly_bms::Interfaces *dalyInterfaces { nullptr };

void dalybms_setup () {

    delay (5 * 1000);
    DEBUG_START ();
    DEBUG_PRINTF ("*** SETUP\n");

    // testRaw ();
    // testOne ();
    testTwo ();

    // clang-format off
    daly_bms::Interfaces::Config *config = new daly_bms::Interfaces::Config {
        .interfaces = std::vector<daly_bms::Interface::Config> {
            daly_bms::Interface::Config {
                .manager = {
                    .id = MANAGER_NAME,
                    .capabilities = daly_bms::Capabilities::Managing + 
                                    daly_bms::Capabilities::TemperatureSensing - 
                                    daly_bms::Capabilities::FirmwareIndex - 
                                    daly_bms::Capabilities::RealTimeClock,
                    .categories = daly_bms::Categories::All,
                    .debugging = daly_bms::Debugging::Errors + 
                                 daly_bms::Debugging::Requests + 
                                 daly_bms::Debugging::Responses,
                },
                .serialId = MANAGER_ID,
                .serialRxPin = MANAGER_PIN_RX,
                .serialTxPin = MANAGER_PIN_TX,
                .enPin = MANAGER_PIN_EN },
            daly_bms::Interface::Config { 
                .manager = {
                    .id = BALANCE_NAME,
                    .capabilities = daly_bms::Capabilities::Balancing + 
                                    daly_bms::Capabilities::TemperatureSensing - 
                                    daly_bms::Capabilities::FirmwareIndex,
                    .categories = daly_bms::Categories::All,
                    .debugging = daly_bms::Debugging::Errors + 
                                 daly_bms::Debugging::Requests + 
                                 daly_bms::Debugging::Responses,
                },
                .serialId = BALANCE_ID,
                .serialRxPin = BALANCE_PIN_RX,
                .serialTxPin = BALANCE_PIN_TX,
                .enPin = BALANCE_PIN_EN } 
        }
    };
    // clang-format on
    dalyInterfaces = new daly_bms::Interfaces (*config);
    if (! dalyInterfaces || ! dalyInterfaces->begin ()) {
        DEBUG_PRINTF ("*** FAILED\n");
        esp_deep_sleep_start ();
    }
}

void dalybms_loop () {
    try {
        DEBUG_PRINTF ("*** LOOP\n");
        if (requestStatus)
            dalyInterfaces->requestStatus ();
        if (requestDiagnostics)
            dalyInterfaces->requestDiagnostics (), dalyInterfaces->updateInitial ();
        processInterval.wait ();
        dalyInterfaces->process ();
        // if (reportData) dalyInterfaces->debugDump();
    } catch (const std::exception &e) {
        DEBUG_PRINTF ("exception: %s\n", e.what ());
    } catch (...) {
        DEBUG_PRINTF ("exception: unknown\n");
    }
}

#ifdef PLATFORMIO
void setup () {
    dalybms_setup ();
}
void loop () {
    dalybms_loop ();
}
#endif

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#endif    // DALYBMS_STANDALONE PLATFORMIO
