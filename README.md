
### Overview 

This is a C++ (currently Arduino platform based, but only minimally platform dependant) interface for the Daly BMS (https://github.com/matthewgream/battery_pack).

It is not officially supported not assisted. It is clean room built from scratch using official and unofficial sources. Tested on a Daly BMS device and balancer.

Modular and extensible using modern C++. Supports multiple manager instances with Capability/Category selectivity of commands. Only supports reads and commands, and does not support writes (yet). 

There did not exist any useful and reliable and comprehensive software interface, and I was not satisfied with the reliability of the WiFi and Bluetooth dongles for the BMS. In addition, the clunky product has a separate balancer with duplicated voltage detection wiring with two separate serial intefaces: the results are "combined" in the Daly BMS App. 

### Architecture

- Builds under Arduino IDE and Platform IO
- Does depend on third-party librarties other than ArduinoJSON (mostly isolated to `DalyBMSConverterJson.hpp`)
- I use single source files (.hpp only, no separation of interface/implementation via. .hpp/cpp)
- namespace `daly_bms`
  - `DalyBMSUtilities.hpp` for generic utilities, including DEBUG definitions
  - `DalyBMSRequestResponse.hpp` for base class request/response frames
  - `DalyBMSRequestResponseTypes.hpp` for specific frame types, as detailed below, with extensive checking
  - `DalyBMSManager.hpp` implements capability/category based request/response transmit/receive for one interface
  - `DalyBMSConnector.hpp` provides HardwareSerial connectivity for the interface manager
  - `DalyBMSConverterDebug.hpp` provides conversion from manager contained request/responses to debug output
  - `DalyBMSConverterJson.hpp` provides conversion  from manager contained request/responses to JSon using ArduinoJson
  - `DalyBMSInterface.hpp` is the user interface assembling multiple managers into a coherent view
  - `main.cpp` is the simple example for two intefaces (Manager and Balancer) with different capabilities
- modern C++ using containers / functional / templates / const and highly modular / separable

### Supported Request/Responses

  - Information
    - RequestResponse_BMS_CONFIG
    - RequestResponse_BMS_HARDWARE
    - RequestResponse_BMS_FIRMWARE
    - RequestResponse_BMS_SOFTWARE
    - RequestResponse_BATTERY_RATINGS
    - RequestResponse_BATTERY_CODE
    - RequestResponse_BATTERY_INFO
    - RequestResponse_BATTERY_STAT
    - RequestResponse_BMS_RTC
  - Thresholds
    - RequestResponse_THRESHOLDS_VOLTAGE
    - RequestResponse_THRESHOLDS_CURRENT
    - RequestResponse_THRESHOLDS_SENSOR
    - RequestResponse_THRESHOLDS_CHARGE
    - RequestResponse_THRESHOLDS_SHORTCIRCUIT
    - RequestResponse_THRESHOLDS_CELL_VOLTAGE
    - RequestResponse_THRESHOLDS_CELL_SENSOR
    - RequestResponse_THRESHOLDS_CELL_BALANCE
  - Status
    - RequestResponse_STATUS
    - RequestResponse_VOLTAGE_MINMAX
    - RequestResponse_SENSOR_MINMAX
    - RequestResponse_MOSFET
    - RequestResponse_INFORMATION
    - RequestResponse_FAILURE
  - Diagnostics
    - RequestResponse_VOLTAGES
    - RequestResponse_SENSORS
    - RequestResponse_BALANCES
  - Commands
    - RequestResponse_RESET
    - RequestResponse_MOSFET_DISCHARGE
    - RequestResponse_MOSFET_CHARGE

### Sources

The following sources were used, in addition to some reverse engineering, for the implementation:

- unofficial
  - https://github.com/maland16/daly-bms-uart
  - https://github.com/softwarecrash/Daly2MQTT
  - https://github.com/dreadnought/python-daly-bms
  - https://diysolarforum.com/threads/decoding-the-daly-smartbms-protocol.21898
  - https://diysolarforum.com/threads/daly-bms-communication-protocol.65439
  - https://diysolarforum.com/resources/daly-smart-bms-manual-and-documentation.48
- official
  - https://robu.in/wp-content/uploads/2021/10/Daly-CAN-Communications-Protocol-V1.0-1.pdf
  - https://www.dalybms.com/download-pc-software

### Hardware

The serial interface (see https://diysolarforum.com/threads/daly-bms-led-12v-port-wiring.67951) is 3.3V so can be directly wired to GPIO. Note that the "S1" pin called "Button activation" is an active-LOW enabler of the interface if it sleeping. For example, the Daly SMART BMS 3-inch display screen has an activation button: if the BMS is sleeping, when this button is pushed, initially the display will show zero values until the BMS is awake and responds with correct information), therefore you need to use this as an EN. I'm not sure yet whether it can be held LOW for the duration or needs some cycling. So you need four wires : TX, RX, EN and GND. THis is the case for both the UART port "LED" interfaces.
