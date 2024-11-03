
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#pragma once

#include "DalyBMSUtilities.hpp"
#include "DalyBMSRequestResponseBase.hpp"
#include "DalyBMSRequestResponseTypes.hpp"

#include <HardwareSerial.h>

#include <vector>
#include <map>

namespace daly_bms {

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

enum Direction { Transmit, Receive };
using RequestResponseFrame_Handlerable = std::pair <const RequestResponseFrame&, Direction>;
class RequestResponseFrame_Receiver: public Handlerable <RequestResponseFrame_Handlerable, bool> {

public:
    virtual void begin() {}
    void write(const RequestResponseFrame& frame) {
        notifyHandlers (Handler::Type (frame, Direction::Transmit));
        writeBytes(frame.data(), frame.size());
        read();
    }
    void process() {
        read();
    }
protected:
    virtual bool readByte(uint8_t* byte) = 0;
    virtual bool writeBytes(const uint8_t* data, const size_t size) = 0;

    void read() {
        uint8_t byte;
        while (readByte(&byte))
            if ((this->*_readStateProcessors[static_cast<size_t>(_readState)])(byte))
                readStateStart();
    }

    enum class ReadState {
        WaitingForStart = 0,
        ProcessingHeader = 1,
        ProcessingContent = 2
    };
    void readStateStart() {
        _readState = ReadState::WaitingForStart;
        _readOffset = RequestResponseFrame::Constants::OFFSET_BYTE_START;  
    }
    bool readStateWaitingForStart(uint8_t byte) {
        if (byte == RequestResponseFrame::Constants::VALUE_BYTE_START) {
            _readOffset = RequestResponseFrame::Constants::OFFSET_ADDRESS;
            _readFrame[RequestResponseFrame::Constants::OFFSET_BYTE_START] = byte;
            _readState = ReadState::ProcessingHeader;
        }
        return false;
    }
    bool readStateProcessingHeader(uint8_t byte) {
        _readFrame[_readOffset] = byte;
        if (++_readOffset < RequestResponseFrame::Constants::SIZE_HEADER)
            return false;
        if (_readFrame[RequestResponseFrame::Constants::OFFSET_ADDRESS] >= RequestResponseFrame::Constants::VALUE_ADDRESS_SLAVE)
            return true;
        _readState = ReadState::ProcessingContent;
        return false;
    }
    bool readStateProcessingContent(uint8_t byte) {
        _readFrame[_readOffset] = byte;
        if (++_readOffset < RequestResponseFrame::Constants::SIZE_FRAME)
            return false;
        if (_readFrame.valid())
            notifyHandlers(Handler::Type(_readFrame, Direction::Receive));
        return true;
    }

private:
    using ReadStateProcessor = bool (RequestResponseFrame_Receiver::*)(uint8_t);
    static constexpr ReadStateProcessor _readStateProcessors[] = {
        &RequestResponseFrame_Receiver::readStateWaitingForStart,
        &RequestResponseFrame_Receiver::readStateProcessingHeader,
        &RequestResponseFrame_Receiver::readStateProcessingContent
    };
    ReadState _readState{ReadState::WaitingForStart};
    size_t _readOffset{RequestResponseFrame::Constants::OFFSET_BYTE_START};
    RequestResponseFrame _readFrame{};
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class RequestResponseManager: public Handlerable <RequestResponse&, bool> {
public:
    bool receiveFrame(const RequestResponseFrame& frame) {
        auto it = _requestsMap.find(frame.getCommand());
        if (it != _requestsMap.end() && it->second->processResponse(frame))
            if (it->second->isValid())
                return notifyHandlers(*it->second);
        return false;
    }

    explicit RequestResponseManager(const std::vector<RequestResponse*>& requests)
        : _requests(requests) {
        for (auto& request : _requests)
            _requestsMap[request->getCommand()] = request;
    }

    const std::vector<RequestResponse*> _requests{};
    std::map<uint8_t, RequestResponse*> _requestsMap{};
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

enum class Capabilities {
    None = 0,
    Managing = 1 << 0,
    Balancing = 1 << 1,
    TemperatureSensing = 1 << 3,
    All = Managing | Balancing | TemperatureSensing
};
enum class Categories {
    None = 0,
    Information = 1 << 0,
    Thresholds = 1 << 1,
    Status = 1 << 2,
    Diagnostics = 1 << 3,
    Commands = 1 << 4,
    All = Information | Thresholds | Status | Diagnostics | Commands
};

inline Capabilities operator|(Capabilities a, Capabilities b) {
    return static_cast<Capabilities>(static_cast<int>(a) | static_cast<int>(b));
}
inline Capabilities operator&(Capabilities a, Capabilities b) {
    return static_cast<Capabilities>(static_cast<int>(a) & static_cast<int>(b));
}
inline Categories operator|(Categories a, Categories b) {
    return static_cast<Categories>(static_cast<int>(a) | static_cast<int>(b));
}
inline Categories operator&(Categories a, Categories b) {
    return static_cast<Categories>(static_cast<int>(a) & static_cast<int>(b));
}

inline String toString(Capabilities capability) {
    switch (capability) {
        case Capabilities::None: return "None";
        case Capabilities::Managing: return "Managing";
        case Capabilities::Balancing: return "Balancing";
        case Capabilities::TemperatureSensing: return "TemperatureSensing";
        case Capabilities::All: return "All";
        default: return "Unknown";
    }
}
inline String toString(Categories category) {
    switch (category) {
        case Categories::None: return "None";
        case Categories::Information: return "Information";
        case Categories::Thresholds: return "Thresholds";
        case Categories::Status: return "Status";
        case Categories::Diagnostics: return "Diagnostics";
        case Categories::Commands: return "Commands";
        case Categories::All: return "All";
        default: return "Unknown";
    }
}

// -----------------------------------------------------------------------------------------------

typedef struct {
    String id;
    Capabilities capabilities;
    Categories categories;
} Config;

// -----------------------------------------------------------------------------------------------

class Interface;
void dumpDebug(const Interface&);

class Interface {

public:
    using Capabilities = daly_bms::Capabilities;
    using Categories = daly_bms::Categories;

    const Config& getConfig () const{ return config; }
    bool isEnabled(const RequestResponse* response) const {
        auto it = std::find_if(requestResponsesSpecifications.begin(), requestResponsesSpecifications.end(), [response](const RequestResponseSpecification& item) {
            return &item.request == response;
        });
        return it != requestResponsesSpecifications.end() && (it->capabilities & config.capabilities) != Capabilities::None;
    }
    bool isEnabled(const Categories category) const {
        return (config.categories & category) != Categories::None;
    };

    using Connector = RequestResponseFrame::Receiver;

    struct Information {    // unofficial
        RequestResponse_BMS_HARDWARE_CONFIG hardware_config;
        RequestResponse_BMS_HARDWARE_VERSION hardware_version;
        RequestResponse_BMS_FIRMWARE_INDEX firmware_index;
        RequestResponse_BMS_SOFTWARE_VERSION software_version;
        RequestResponse_BATTERY_RATINGS battery_ratings;
        RequestResponse_BATTERY_CODE battery_code;
        RequestResponse_BATTERY_INFO battery_info;
        RequestResponse_BATTERY_STAT battery_stat;
        RequestResponse_BMS_RTC rtc;
    } information{};
    struct Thresholds {    // unofficial
        RequestResponse_PACK_VOLTAGES_THRESHOLDS pack_voltages;
        RequestResponse_PACK_CURRENTS_THRESHOLDS pack_currents;
        RequestResponse_PACK_TEMPERATURE_THRESHOLDS pack_temperatures;
        RequestResponse_PACK_SOC_THRESHOLDS pack_soc;
        RequestResponse_CELL_VOLTAGES_THRESHOLDS cell_voltages;
        RequestResponse_CELL_SENSORS_THRESHOLDS cell_sensors;
        RequestResponse_CELL_BALANCES_THRESHOLDS cell_balances;
        RequestResponse_PACK_SHORTCIRCUIT_THRESHOLDS pack_shortcircuit;
    } thresholds{};
    struct Status {
        RequestResponse_PACK_STATUS pack;
        RequestResponse_CELL_VOLTAGES_MINMAX cell_voltages;
        RequestResponse_CELL_TEMPERATURES_MINMAX cell_temperatures;
        RequestResponse_MOSFET_STATUS fets;
        RequestResponse_PACK_INFORMATION info;
        RequestResponse_FAILURE_STATUS failures;
    } status{};
    struct Diagnostics {
        RequestResponse_CELL_VOLTAGES voltages;
        RequestResponse_CELL_TEMPERATURES temperatures;
        RequestResponse_CELL_BALANCES balances;
    } diagnostics{};
    struct Commands {    // unofficial
        RequestResponse_BMS_RESET reset;
        RequestResponse_MOSFET_DISCHARGE discharge;
        RequestResponse_MOSFET_CHARGE charge;
    } commands{};

    explicit Interface(const Config& conf, Connector& connector)
        : requestResponsesSpecifications({

              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, information.hardware_config },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, information.hardware_version },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, information.firmware_index },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, information.software_version },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, information.battery_ratings },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, information.battery_code },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, information.battery_info },
              { Categories::Information, Capabilities::Managing, information.battery_stat },
              { Categories::Information, Capabilities::Managing | Capabilities::Balancing, information.rtc },

              { Categories::Thresholds, Capabilities::Managing | Capabilities::Balancing, thresholds.pack_voltages },
              { Categories::Thresholds, Capabilities::Managing | Capabilities::Balancing, thresholds.pack_currents },
              { Categories::Thresholds, Capabilities::TemperatureSensing, thresholds.pack_temperatures },
              { Categories::Thresholds, Capabilities::Managing, thresholds.pack_soc },
              { Categories::Thresholds, Capabilities::Managing | Capabilities::Balancing, thresholds.cell_voltages },
              { Categories::Thresholds, Capabilities::TemperatureSensing, thresholds.cell_sensors },
              { Categories::Thresholds, Capabilities::Balancing, thresholds.cell_balances },
              { Categories::Thresholds, Capabilities::Managing, thresholds.pack_shortcircuit },

              { Categories::Status, Capabilities::Managing, status.pack },
              { Categories::Status, Capabilities::Managing | Capabilities::Balancing, status.cell_voltages },
              { Categories::Status, Capabilities::TemperatureSensing, status.cell_temperatures },
              { Categories::Status, Capabilities::Managing, status.fets },
              { Categories::Status, Capabilities::Managing | Capabilities::Balancing, status.info },
              { Categories::Status, Capabilities::Managing, status.failures },

              { Categories::Diagnostics, Capabilities::Managing | Capabilities::Balancing, diagnostics.voltages },
              { Categories::Diagnostics, Capabilities::TemperatureSensing, diagnostics.temperatures },
              { Categories::Diagnostics, Capabilities::Balancing, diagnostics.balances },

              { Categories::Commands, Capabilities::Managing | Capabilities::Balancing, commands.reset },
              { Categories::Commands, Capabilities::Managing, commands.charge },
              { Categories::Commands, Capabilities::Managing, commands.discharge }

          }),
          config(conf), connector(connector), manager(buildRequestResponses(config.capabilities)) {

        struct ResponseHandler : RequestResponseManager::Handler {
            Interface& interface;
            ResponseHandler (Interface& i): interface (i) {}
            bool handle(RequestResponse& response) override {
                if (response.getCommand() == interface.status.info) {
                    interface.diagnostics.voltages.setCount(interface.status.info.numberOfCells);
                    interface.diagnostics.temperatures.setCount(interface.status.info.numberOfSensors);
                    interface.diagnostics.balances.setCount(interface.status.info.numberOfCells);
                    interface.manager.unregisterHandler(this);
                    delete this;
                }
                return true;
            }
        };
        manager.registerHandler(new ResponseHandler(*this));

        struct FrameHandler: RequestResponseFrame::Receiver::Handler {
            Interface& interface;
            FrameHandler (Interface& i): interface (i) {}
            bool handle (RequestResponseFrame::Receiver::Handler::Type frame) {
                DEBUG_PRINTF("DalyBMS<%s>: %s: %s\n", interface.config.id, frame.second == Direction::Transmit ? "send" : "recv", frame.first.toString().c_str());
                if (frame.second == Direction::Receive)
                    interface.manager.receiveFrame(frame.first);
                return frame.second == Direction::Receive;
            }
        };
        connector.registerHandler (new FrameHandler (*this));
    }

    void begin() {
        DEBUG_PRINTF("DalyBMS<%s>: begin\n", config.id);
        connector.begin();
        requestInitial();
    }
    void loop() {
        connector.process();
    }

    void issue(RequestResponse& request) {
        if (request.isRequestable())
            connector.write(request.prepareRequest());
    }
    void requestStatus() {
        request(Categories::Status);
    }
    void requestDiagnostics() {
        request(Categories::Diagnostics);
    }

protected:
    void requestInformation() {
        request(Categories::Information);
    }
    void requestThresholds() {
        request(Categories::Thresholds);
    }
public:
    void request(const Categories category) {
        if (!isEnabled(category)) return;
        DEBUG_PRINTF("DalyBMS<%s>: request%s\n", config.id, toString(category).c_str());
        auto it = requestResponses.find(category);
        if (it != requestResponses.end())
            for (auto r : it->second)
                issue(*r);
    }
    void requestInitial() {
        DEBUG_PRINTF("DalyBMS<%s>: requestInitial\n", config.id);
        for (auto category : { Categories::Information, Categories::Thresholds, Categories::Status, Categories::Diagnostics, Categories::Commands })
            request(category);
    }
    void dumpDebug() const {
        return daly_bms::dumpDebug(*this);
    }

private:

    std::map<Categories, std::vector<RequestResponse*>> requestResponses;
    struct RequestResponseSpecification {
        Categories category;
        Capabilities capabilities;
        RequestResponse& request;
    };
    std::vector<RequestResponseSpecification> requestResponsesSpecifications;

    std::vector<RequestResponse*> buildRequestResponses(const Capabilities capabilities) {
        std::vector<RequestResponse*> r;
        for (const auto& item : requestResponsesSpecifications)
            if ((item.capabilities & capabilities) != Capabilities::None)
                requestResponses[item.category].push_back(&item.request);
        for (const auto& [category, requests] : requestResponses)
            r.insert(r.end(), requests.begin(), requests.end());
        return r;
    }

    const Config& config;
    Connector& connector;
    RequestResponseManager manager;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class HardwareSerialConnector : public Interface::Connector {
public:
    explicit HardwareSerialConnector(HardwareSerial& serial)
        : _serial(serial) {
    }
protected:
    void begin() override {
    }
    bool readByte(uint8_t* byte) override {
        if (_serial.available() > 0) {
            *byte = _serial.read();
            return true;
        } else DEBUG_PRINTF("no data\n");
        return false;
    }
    bool writeBytes(const uint8_t* data, const size_t size) override {
        return _serial.write(data, size) == size;
    }
private:
    HardwareSerial& _serial;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
