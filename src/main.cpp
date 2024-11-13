
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

static constexpr int MANAGER_PIN_RX = GPIO_NUM_5, MANAGER_PIN_TX = GPIO_NUM_6, MANAGER_PIN_EN = GPIO_NUM_7, MANAGER_ID = 1;
static constexpr const char *MANAGER_NAME = "manager";
static constexpr int BALANCE_PIN_RX = GPIO_NUM_15, BALANCE_PIN_TX = GPIO_NUM_16, BALANCE_PIN_EN = GPIO_NUM_17, BALANCE_ID = 2;
static constexpr const char *BALANCE_NAME = "balance";

// -----------------------------------------------------------------------------------------------

void testRaw () {

    const int serialId = MANAGER_ID, serialRxPin = MANAGER_PIN_RX, serialTxPin = MANAGER_PIN_TX, enPin = MANAGER_PIN_EN;

    try {
        HardwareSerial serial (serialId);
        serial.begin (daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_BAUD, daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_CONFIG, serialRxPin, serialTxPin);
        if (enPin >= 0)
            digitalWrite (enPin, LOW);

        while (1) {
            uint8_t command [13] = { 0xA5, 0x40, 0x90, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7D };
            serial.write (command, 13);
            while (serial.available () > 0) {
                uint8_t byte = serial.read ();
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

    // const int serialId = MANAGER_ID, serialRxPin = MANAGER_PIN_RX, serialTxPin = MANAGER_PIN_TX, enPin = MANAGER_PIN_EN;
    // daly_bms::Config config = { .id = "manager", .capabilities = daly_bms::Capabilities::All, .categories = daly_bms::Categories::All };
    const int serialId = BALANCE_ID, serialRxPin = BALANCE_PIN_RX, serialTxPin = BALANCE_PIN_TX, enPin = BALANCE_PIN_EN;
    daly_bms::Manager::Config config = { .id = "balance", .capabilities = daly_bms::Capabilities::All, .categories = daly_bms::Categories::All };

    HardwareSerial serial (serialId);
    serial.begin (daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_BAUD, daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_CONFIG, serialRxPin, serialTxPin);
    if (enPin >= 0)
        digitalWrite (enPin, LOW);
    daly_bms::HardwareSerialConnector connector (serial);
    daly_bms::Manager manager (config, connector);
    manager.begin ();

    while (1) {
        auto &request = manager.information.rtc;
        // manager.issue (manager.status.info); // required to capture numbers for diagnostics request/responses
        manager.issue (request);
        delay (5000);
        manager.process ();
        if (request.isValid ()) {
            DEBUG_PRINTF ("---> \n");
            request.debugDump ();
            DEBUG_PRINTF ("<--- \n");
        }
        DEBUG_PRINTF (".\n");
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

    using DalyBMSInterfaces = daly_bms::Interfaces;

    DalyBMSInterfaces::Config *config = new DalyBMSInterfaces::Config {
        .interfaces = std::vector<daly_bms::Interface::Config> {
                                                                daly_bms::Interface::Config {
                .manager = {
                    .id = MANAGER_NAME,
                    .capabilities = daly_bms::Capabilities::Managing + daly_bms::Capabilities::TemperatureSensing - daly_bms::Capabilities::FirmwareIndex - daly_bms::Capabilities::RealTimeClock,
                    .categories = daly_bms::Categories::All,
                    .debugging = daly_bms::Debugging::Errors + daly_bms::Debugging::Requests + daly_bms::Debugging::Responses,
                },
                .serialId = MANAGER_ID,
                .serialRxPin = MANAGER_PIN_RX,
                .serialTxPin = MANAGER_PIN_TX,
                .enPin = MANAGER_PIN_EN },
                                                                daly_bms::Interface::Config { .manager = {
                                              .id = BALANCE_NAME,
                                              .capabilities = daly_bms::Capabilities::Balancing + daly_bms::Capabilities::TemperatureSensing - daly_bms::Capabilities::FirmwareIndex,
                                              .categories = daly_bms::Categories::All,
                                              .debugging = daly_bms::Debugging::Errors + daly_bms::Debugging::Requests + daly_bms::Debugging::Responses,
                                          },
                                          .serialId = BALANCE_ID,
                                          .serialRxPin = BALANCE_PIN_RX,
                                          .serialTxPin = BALANCE_PIN_TX,
                                          .enPin = BALANCE_PIN_EN } }
    };
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
