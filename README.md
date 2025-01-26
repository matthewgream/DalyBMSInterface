
### Overview 

This is a C++ (currently Arduino platform based, but only minimally platform dependant) interface for the Daly BMS (https://www.dalybms.com) as used in my own battery pack project (https://github.com/matthewgream/battery_pack).

It is not officially supported not assisted. It is clean room built from scratch using official and unofficial sources and tested on a Daly Smart BMS device and balancer. It is not tested to other Daly products.

The design and implementation is modular and extensible using modern C++. It supports multiple manager instances with capability/category selectivity of commands. For now, there are only reads and selected commands, not writes. 

There did not exist any useful and reliable and comprehensive software interface, and I was not satisfied with the reliability of the WiFi and Bluetooth dongles for the BMS. In addition, the clunky product has a separate balancer with duplicated voltage detection wiring with two separate serial intefaces: the results are "combined" in the Daly BMS App. The best I could find was the so called "open" DalyBMS WiFi dongle, but it was not suited to my needs and the author would not share any Daly interface code (even though he extensively relies on open source).

### License

The LICENSE is for non-commercial attribution and share-alike. There are no warranties on this basis. I am happy for people to use this to extend the universe of open solutions, but if you desire to make gains from it, please contact me to discuss alternative licensing arrangements. This is not a derived work so I am the exclusive rights holder.

### Architecture

- builds under Arduino IDE and Platform IO
- no dependency on third-party librarties other than ArduinoJSON (mostly isolated to `DalyBMSConverterJson.hpp`)
- uses single source files (.hpp only, no separation of interface/implementation via. .hpp/cpp)
- contained in namespace `daly_bms`
  - `DalyBMSUtilities.hpp` for generic utilities, including DEBUG definitions
  - `DalyBMSRequestResponse.hpp` for base class request/response frames
  - `DalyBMSRequestResponseTypes.hpp` for specific frame types, as detailed below, with extensive checking
  - `DalyBMSManager.hpp` implements capability/category based request/response transmit/receive for one interface
  - `DalyBMSConnector.hpp` provides HardwareSerial connectivity for the interface manager
  - `DalyBMSConverterDebug.hpp` provides conversion from manager contained request/responses to debug output
  - `DalyBMSConverterJson.hpp` provides conversion  from manager contained request/responses to JSon using ArduinoJson
  - `DalyBMSInterface.hpp` is the user interface assembling multiple managers into a coherent view
  - `main.cpp` is the simple example for two intefaces (Manager and Balancer) with different capabilities
- modern C++ using containers / functional / templates / references / const and highly modular / separable
- built to balance performance, modularity, extensibility, robustness. code is simply autoformatted.

### Supported Request/Responses

  - only ranges 0x90 to 0x98 are "officially" documented by Daly (see https://robu.in/wp-content/uploads/2021/10/Daly-CAN-Communications-Protocol-V1.0-1.pdf), the others are reverse engineered
  - these are my namings, groupings and ordering for what I think is sensible
  - **Information** ... needed (if needed) once per session, read-only
    - RequestResponse_BMS_CONFIG (0x51)
    - RequestResponse_BMS_HARDWARE (0x63)
    - RequestResponse_BMS_FIRMWARE (0x54)
    - RequestResponse_BMS_SOFTWARE (0x62)
    - RequestResponse_BATTERY_RATINGS (0x50)
    - RequestResponse_BATTERY_CODE (0x57)
    - RequestResponse_BATTERY_INFO (0x53)
    - RequestResponse_BATTERY_STAT (0x52)
    - RequestResponse_BMS_RTC (0x61)
  - **Thresholds** ... needed (if needed) once per session, read/write (likely)
    - RequestResponse_THRESHOLDS_VOLTAGE (0x5A)
    - RequestResponse_THRESHOLDS_CURRENT (0x5B)
    - RequestResponse_THRESHOLDS_SENSOR (0x5C)
    - RequestResponse_THRESHOLDS_CHARGE (0x5D)
    - RequestResponse_THRESHOLDS_SHORTCIRCUIT (0x60)
    - RequestResponse_THRESHOLDS_CELL_VOLTAGE (0x59)
    - RequestResponse_THRESHOLDS_CELL_SENSOR (0x5C)
    - RequestResponse_THRESHOLDS_CELL_BALANCE (0x5F)
  - **Status** ... requested (as needed) at some reasonable frequency, particuarly 0x90 and 0x98, read-only
    - RequestResponse_STATUS (0x90) *[OFFICIAL]*
    - RequestResponse_VOLTAGE_MINMAX (0x91) *[OFFICIAL]*
    - RequestResponse_SENSOR_MINMAX (0x92) *[OFFICIAL]*
    - RequestResponse_MOSFET (0x93) *[OFFICIAL]*
    - RequestResponse_INFORMATION (0x94) *[OFFICIAL]*
    - RequestResponse_FAILURE (0x98) *[OFFICIAL]*
  - **Diagnostics** ... requested (if needed) at some lesser frequency, read-only
    - RequestResponse_VOLTAGES (0x95) *[OFFICIAL]*
    - RequestResponse_SENSORS (0x96) *[OFFICIAL]*
    - RequestResponse_BALANCES (0x97) *[OFFICIAL]*
  - **Commands** ... issued (if required), write-only (with confirmation)
    - RequestResponse_RESET (0x00)
    - RequestResponse_MOSFET_DISCHARGE (0xD9)
    - RequestResponse_MOSFET_CHARGE (0xDA)

### Sources

The following sources were used, in addition to some reverse engineering, for the implementation:

- official
  - https://robu.in/wp-content/uploads/2021/10/Daly-CAN-Communications-Protocol-V1.0-1.pdf
  - https://www.dalybms.com/download-pc-software
- unofficial
  - https://github.com/maland16/daly-bms-uart
  - https://github.com/softwarecrash/Daly2MQTT
  - https://github.com/dreadnought/python-daly-bms
  - https://diysolarforum.com/threads/decoding-the-daly-smartbms-protocol.21898
  - https://diysolarforum.com/threads/daly-bms-communication-protocol.65439
  - https://diysolarforum.com/resources/daly-smart-bms-manual-and-documentation.48

### Hardware

The serial interface (see https://diysolarforum.com/threads/daly-bms-led-12v-port-wiring.67951) is 3V3 so can be directly wired to GPIO. Note that the "S1" pin called "Button activation" is an active-LOW enabler of the interface if it sleeping. For example, the Daly SMART BMS 3-inch display screen has an activation button: if the BMS is sleeping, when this button is pushed, initially the display will show zero values until the BMS is awake and responds with correct information), therefore you need to use this as an EN. It seems like you just hold this low for the duration (there is no timeout). This, you need four wires: TX, RX, EN and GND. This is the case for both the UART port "LED" interfaces. I recommend being very careful with the hardware as the serial interface also carries a 12V line and if you miswire this, you will kill your microcontroller and possibly damage anything connected to the USB (your development machine).
