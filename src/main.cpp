
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

typedef unsigned long interval_t;
typedef unsigned long counter_t;

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

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

#include "DalyBMSManager.hpp"

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

void testDirect() {

    const int serialId = MANAGER_ID, serialRxPin = MANAGER_PIN_RX, serialTxPin = MANAGER_PIN_TX, enPin = MANAGER_PIN_EN;
    HardwareSerial* serial;

    daly_bms::exception_catcher([&] {
        serial = new HardwareSerial(serialId);
        serial->setRxBufferSize(1024);
        serial->begin(9600, SERIAL_8N1, serialRxPin, serialTxPin);
        if (enPin >= 0) digitalWrite(enPin, HIGH);

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

Intervalable processGate(5 * 1000),
    requestStatus(15 * 1000), requestDiagnostics(30 * 1000), reportData(10 * 1000);

DalyAggregationManager* dalyManager{nullptr};

void setup() {

    delay(5 * 1000);
    DEBUG_START();
    DEBUG_PRINTF("*** SETUP\n");
    delay(60 * 1000 * 1000);

    //testDirect ();

    DalyAggregationManager::Config* config = new DalyAggregationManager::Config({
        .instances = {
            { .daly = {
                .id = MANAGER_NAME,
                .capabilities = DalyAggregationManager::DalyCapabilities::All,
                .categories = DalyAggregationManager::DalyCategories::All },
                .serialId = MANAGER_ID,
                .serialRxPin = MANAGER_PIN_RX,
                .serialTxPin = MANAGER_PIN_TX,
              .enPin = MANAGER_PIN_EN },
            { .daly = { 
                .id = BALANCE_NAME, 
                .capabilities = DalyAggregationManager::DalyCapabilities::All, 
                .categories = DalyAggregationManager::DalyCategories::All }, 
                .serialId = BALANCE_ID, 
                .serialRxPin = BALANCE_PIN_RX, 
                .serialTxPin = BALANCE_PIN_TX, 
                .enPin = BALANCE_PIN_EN },
        }
    });
    dalyManager = new DalyAggregationManager(*config);
    if (!dalyManager || !dalyManager->begin()) {
        DEBUG_PRINTF("*** FAILED\n");
        esp_deep_sleep_start();
    }
}

void loop() {
    processGate.wait();
    daly_bms::exception_catcher([&] {
        DEBUG_PRINTF("*** LOOP\n");
        dalyManager->loop();
        if (requestStatus) dalyManager->requestStatus();
        if (requestDiagnostics) dalyManager->requestDiagnostics();
        if (reportData) dalyManager->dumpDebug();
    });
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
