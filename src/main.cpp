
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

typedef unsigned long interval_t;
class Intervalable {
    interval_t _interval, _previous{};
public:
    explicit Intervalable(const interval_t interval = 0) : _interval(interval) {}
    operator bool() {
        const interval_t current = millis();
        if (current - _previous > _interval) {
            _previous = current;
            return true;
        }
        return false;
    }
    void wait() {
        const interval_t current = millis();
        if (current - _previous < _interval)
            delay(_interval - (current - _previous));
        _previous = millis();
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "DalyBMSInstance.hpp"

// -----------------------------------------------------------------------------------------------

// TEST_DEVICE = ESP32-S3-DEVKITC-1
// https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html
// device 1 - GPIO 5, 6, 7 (rx, tx, en) + gnd
// device 2 - GPIO 15, 16, 17 (rx, tx, en) + gnd

static constexpr int MANAGER_PIN_RX = GPIO_NUM_5, MANAGER_PIN_TX = GPIO_NUM_6, MANAGER_PIN_EN = GPIO_NUM_7, MANAGER_ID = 1;
static constexpr const char* MANAGER_NAME = "manager";
static constexpr int BALANCE_PIN_RX = GPIO_NUM_15, BALANCE_PIN_TX = GPIO_NUM_16, BALANCE_PIN_EN = GPIO_NUM_17, BALANCE_ID = 2;
static constexpr const char* BALANCE_NAME = "balance";

// -----------------------------------------------------------------------------------------------

void testRaw() {

    const int serialId = MANAGER_ID, serialRxPin = MANAGER_PIN_RX, serialTxPin = MANAGER_PIN_TX, enPin = MANAGER_PIN_EN;
    HardwareSerial* serial;

    daly_bms::exception_catcher([&] {
        HardwareSerial serial(serialId);
        serial.begin(daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_BAUD, daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_CONFIG, serialRxPin, serialTxPin);
        if (enPin >= 0) digitalWrite(enPin, HIGH);

        while (1) {
            uint8_t command [13] = { 0xA5, 0x40 , 0x90 , 0x08 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x7D };
            serial.write(command, 13);
            while (serial.available() > 0) {
                uint8_t byte = serial.read();
                DEBUG_PRINTF("<%02X>", byte);
            }
            DEBUG_PRINTF("*\n");
            delay(5000);
        }
    });
}

// -----------------------------------------------------------------------------------------------

void testOne() {

    // const int serialId = MANAGER_ID, serialRxPin = MANAGER_PIN_RX, serialTxPin = MANAGER_PIN_TX, enPin = MANAGER_PIN_EN;
    // daly_bms::Config config = { .id = "manager", .capabilities = daly_bms::Capabilities::All, .categories = daly_bms::Categories::All };
    const int serialId = BALANCE_ID, serialRxPin = BALANCE_PIN_RX, serialTxPin = BALANCE_PIN_TX, enPin = BALANCE_PIN_EN;
    daly_bms::Interface::Config config = { .id = "balance", .capabilities = daly_bms::Capabilities::All, .categories = daly_bms::Categories::All };

    HardwareSerial serial (serialId);
    serial.begin(daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_BAUD, daly_bms::HardwareSerialConnector::DEFAULT_SERIAL_CONFIG, serialRxPin, serialTxPin);
    daly_bms::HardwareSerialConnector connector (serial);
    daly_bms::Interface interface (config, connector);
    interface.begin ();

    while (1) {
        auto& request = interface.information.rtc;
        // interface.issue (interface.status.info); // required to capture numbers for diagnostics request/responses
        interface.issue (request);
        delay (5000);
        interface.process ();
        if (request.isValid ()) {
            DEBUG_PRINTF ("---> \n");
            request.debugDump ();
            DEBUG_PRINTF ("<--- \n");
        }
        DEBUG_PRINTF (".\n");
    }
}

// -----------------------------------------------------------------------------------------------

Intervalable processInterval (5 * 1000), requestStatus(15 * 1000), requestDiagnostics(30 * 1000), reportData(30 * 1000);

daly_bms::Instances* dalyInstances{nullptr};

void setup() {

    delay(5 * 1000);
    DEBUG_START();
    DEBUG_PRINTF("*** SETUP\n");

    // testRaw ();
    // testOne ();

    using DalyBMSInstances = daly_bms::Instances;

    DalyBMSInstances::Config* config = new DalyBMSInstances::Config{
        .instances = std::vector<daly_bms::Instance::Config>{
            daly_bms::Instance::Config{ 
                .daly = {
                    .id = MANAGER_NAME,
                    .capabilities = daly_bms::Capabilities::Managing  + daly_bms::Capabilities::TemperatureSensing - daly_bms::Capabilities::FirmwareIndex - daly_bms::Capabilities::RealTimeClock,
                    .categories = daly_bms::Categories::All,
                    .debugging = daly_bms::Debugging::Errors + daly_bms::Debugging::Requests + daly_bms::Debugging::Responses,
                },
                .serialId = MANAGER_ID, .serialRxPin = MANAGER_PIN_RX, .serialTxPin = MANAGER_PIN_TX, .enPin = MANAGER_PIN_EN 
            },
            daly_bms::Instance::Config{ 
                .daly = { 
                    .id = BALANCE_NAME, 
                    .capabilities = daly_bms::Capabilities::Balancing + daly_bms::Capabilities::TemperatureSensing - daly_bms::Capabilities::FirmwareIndex, 
                    .categories = daly_bms::Categories::All,
                    .debugging = daly_bms::Debugging::Errors + daly_bms::Debugging::Requests + daly_bms::Debugging::Responses,
                }, 
                .serialId = BALANCE_ID, .serialRxPin = BALANCE_PIN_RX, .serialTxPin = BALANCE_PIN_TX, .enPin = BALANCE_PIN_EN 
            }
        }
    };
    dalyInstances = new daly_bms::Instances(*config);
    if (!dalyInstances || !dalyInstances->begin()) {
        DEBUG_PRINTF("*** FAILED\n");
        esp_deep_sleep_start();
    }

}

void loop() {
    daly_bms::exception_catcher([&] {
        DEBUG_PRINTF("*** LOOP\n");
        if (requestStatus) dalyInstances->requestStatus();
        if (requestDiagnostics) dalyInstances->requestDiagnostics(), dalyInstances->updateInitial();
        processInterval.wait();
        dalyInstances->process();
        // if (reportData) dalyInstances->debugDump();
    });
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
