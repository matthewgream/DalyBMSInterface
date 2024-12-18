
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifndef DALYBMS_FLATFILES
#pragma once
#include "DalyBMSUtilities.hpp"
#include "DalyBMSRequestResponse.hpp"
#include "DalyBMSRequestResponseTypes.hpp"
#endif

#include <vector>
#include <map>

namespace daly_bms {

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

enum Direction { Transmit,
                 Receive,
                 Error };    // XXX rename
inline String toString (const Direction &direction) {
    switch (direction) {
    case Direction::Transmit :
        return "send";
    case Direction::Receive :
        return "recv";
    case Direction::Error :
        return "error";
    default :
        return "unknown";
    }
}

using RequestResponseFrame_Handlerable = std::pair<const RequestResponseFrame &, Direction>;
class RequestResponseFrame_Receiver : public Handlerable<RequestResponseFrame_Handlerable, bool> {

public:
    virtual void begin () { }
    void write (const RequestResponseFrame &frame) {
        notifyHandlers (Handler::Type (frame, Direction::Transmit));
        writeBytes (frame.data (), frame.size ());
        read ();
    }
    void process () {
        read ();
    }

protected:
    virtual bool readByte (uint8_t *byte) = 0;
    virtual bool writeBytes (const uint8_t *data, const size_t size) = 0;

    void read () {
        uint8_t byte;
        while (readByte (&byte))
            if ((this->*_readStateProcessors [static_cast<size_t> (_readState)]) (byte))
                readStateStart ();
    }

    enum class ReadState {
        WaitingForStart = 0,
        ProcessingHeader = 1,
        ProcessingContent = 2
    };
    void readStateStart () {
        _readState = ReadState::WaitingForStart;
        _readOffset = RequestResponseFrame::Constants::OFFSET_BYTE_START;
    }
    bool readStateWaitingForStart (uint8_t byte) {
        if (byte == RequestResponseFrame::Constants::VALUE_BYTE_START) {
            _readOffset = RequestResponseFrame::Constants::OFFSET_ADDRESS;
            _readFrame [RequestResponseFrame::Constants::OFFSET_BYTE_START] = byte;
            _readState = ReadState::ProcessingHeader;
        }
        return false;
    }
    bool readStateProcessingHeader (uint8_t byte) {
        _readFrame [_readOffset] = byte;
        if (++_readOffset < RequestResponseFrame::Constants::SIZE_HEADER)
            return false;
        if (_readFrame [RequestResponseFrame::Constants::OFFSET_ADDRESS] > RequestResponseFrame::Constants::VALUE_ADDRESS_BMS_MASTER)
            return true;
        _readState = ReadState::ProcessingContent;
        return false;
    }
    bool readStateProcessingContent (uint8_t byte) {
        _readFrame [_readOffset] = byte;
        if (++_readOffset < RequestResponseFrame::Constants::SIZE_FRAME)
            return false;
        notifyHandlers (Handler::Type (_readFrame, _readFrame.valid () ? Direction::Receive : Direction::Error));
        return true;
    }

private:
    using ReadStateProcessor = bool (RequestResponseFrame_Receiver::*) (uint8_t);
    static constexpr ReadStateProcessor _readStateProcessors [] = {
        &RequestResponseFrame_Receiver::readStateWaitingForStart,
        &RequestResponseFrame_Receiver::readStateProcessingHeader,
        &RequestResponseFrame_Receiver::readStateProcessingContent
    };
    ReadState _readState { ReadState::WaitingForStart };
    size_t _readOffset { RequestResponseFrame::Constants::OFFSET_BYTE_START };
    RequestResponseFrame _readFrame {};
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

enum class Debugging {
    None = 0,
    Frames = 1 << 0,
    Requests = 1 << 1,
    Responses = 1 << 2,
    Errors = 1 << 3,
    All = Frames | Requests | Responses | Errors
};
enum class Capabilities {
    None = 0,
    Managing = 1 << 0,
    Balancing = 1 << 1,
    TemperatureSensing = 1 << 3,
    RealTimeClock = 1 << 4,
    FirmwareIndex = 1 << 5,
    All = Managing | Balancing | TemperatureSensing | RealTimeClock | FirmwareIndex
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

template <typename T>
struct is_flags_enum : std::false_type { };

template <>
struct is_flags_enum<Debugging> : std::true_type { };
template <>
struct is_flags_enum<Capabilities> : std::true_type { };
template <>
struct is_flags_enum<Categories> : std::true_type { };

template <typename EnumType>
requires is_flags_enum<EnumType>::value
inline EnumType operator+ (EnumType a, EnumType b) {
    return static_cast<EnumType> (static_cast<int> (a) | static_cast<int> (b));
}
template <typename EnumType>
requires is_flags_enum<EnumType>::value
inline EnumType operator& (EnumType a, EnumType b) {
    return static_cast<EnumType> (static_cast<int> (a) & static_cast<int> (b));
}
template <typename EnumType>
requires is_flags_enum<EnumType>::value
inline EnumType operator- (EnumType a, EnumType b) {
    return static_cast<EnumType> (static_cast<int> (a) & ~static_cast<int> (b));
}

inline String toString (Debugging debugging) {
    switch (debugging) {
    case Debugging::None :
        return "None";
    case Debugging::Requests :
        return "Requests";
    case Debugging::Responses :
        return "Responses";
    case Debugging::Frames :
        return "Frames";
    case Debugging::All :
        return "All";
    default :
        return "Unknown";
    }
}
inline String toString (Capabilities capability) {
    switch (capability) {
    case Capabilities::None :
        return "None";
    case Capabilities::Managing :
        return "Managing";
    case Capabilities::Balancing :
        return "Balancing";
    case Capabilities::TemperatureSensing :
        return "TemperatureSensing";
    case Capabilities::RealTimeClock :
        return "RealTimeClock";
    case Capabilities::FirmwareIndex :
        return "FirmwareIndex";
    case Capabilities::All :
        return "All";
    default :
        return "Unknown";
    }
}
inline String toString (Categories category) {
    switch (category) {
    case Categories::None :
        return "None";
    case Categories::Information :
        return "Information";
    case Categories::Thresholds :
        return "Thresholds";
    case Categories::Status :
        return "Status";
    case Categories::Diagnostics :
        return "Diagnostics";
    case Categories::Commands :
        return "Commands";
    case Categories::All :
        return "All";
    default :
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------------------------

// merge into below
class RequestResponseManager : public Handlerable<RequestResponse &, bool> {
public:
    bool receiveFrame (const RequestResponseFrame &frame) {
        auto it = _requestsMap.find (frame.getCommand ());
        if (it != _requestsMap.end ())
            if (it->second->processResponse (frame))
                if (it->second->isValid ()) {
                    notifyHandlers (*it->second);
                    return true;
                } else {
                    if (it->second->isComplete ())
                        DEBUG_PRINTF ("RequestResponseManager: frame complete but not valid\n");
                }
            else {
                if (it->second->isComplete ())
                    DEBUG_PRINTF ("RequestResponseManager: frame complete but unprocessable\n");
            }
        else {
            DEBUG_PRINTF ("RequestResponseManager: frame handler not found\n");
        }
        return false;
    }

    explicit RequestResponseManager (const std::vector<RequestResponse *> &requests) :
        _requests (requests) {
        for (auto &request : _requests)
            _requestsMap [request->getCommand ()] = request;
    }

    const std::vector<RequestResponse *> _requests {};
    std::map<uint8_t, RequestResponse *> _requestsMap {};
};

// -----------------------------------------------------------------------------------------------

class Manager;
void dumpDebug (const Manager &);

class Manager {

public:
    typedef struct {
        String id;
        Capabilities capabilities { Capabilities::None };
        Categories categories { Categories::All };
        Debugging debugging { Debugging::Errors };
    } Config;

    using Capabilities = daly_bms::Capabilities;
    using Categories = daly_bms::Categories;
    using Debugging = daly_bms::Debugging;

    const Config &getConfig () const {
        return config;
    }
    bool isEnabled (const RequestResponse *response) const {
        auto it = std::find_if (requestResponsesSpecifications.begin (), requestResponsesSpecifications.end (), [response] (const RequestResponseSpecification &item) {
            return &item.request == response;
        });
        return it != requestResponsesSpecifications.end () && (it->capabilities & config.capabilities) != Capabilities::None;
    }
    bool isEnabled (const Categories category) const {
        return (config.categories & category) != Categories::None;
    };
    bool isEnabled (const Debugging debugging) const {
        return (config.debugging & debugging) != Debugging::None;
    };

    using Connector = RequestResponseFrame::Receiver;

    struct Information {    // unofficial
        RequestResponse_BMS_CONFIG config;
        RequestResponse_BMS_HARDWARE hardware;
        RequestResponse_BMS_FIRMWARE firmware;
        RequestResponse_BMS_SOFTWARE software;
        RequestResponse_BATTERY_RATINGS battery_ratings;
        RequestResponse_BATTERY_CODE battery_code;
        RequestResponse_BATTERY_INFO battery_info;
        RequestResponse_BATTERY_STAT battery_stat;
        RequestResponse_BMS_RTC rtc;
    } information {};
    struct Thresholds {    // unofficial
        RequestResponse_THRESHOLDS_VOLTAGE voltage;
        RequestResponse_THRESHOLDS_CURRENT current;
        RequestResponse_THRESHOLDS_SENSOR sensor;
        RequestResponse_THRESHOLDS_CHARGE charge;
        RequestResponse_THRESHOLDS_SHORTCIRCUIT shortcircuit;
        RequestResponse_THRESHOLDS_CELL_VOLTAGE cell_voltage;
        RequestResponse_THRESHOLDS_CELL_SENSOR cell_sensor;
        RequestResponse_THRESHOLDS_CELL_BALANCE cell_balance;
    } thresholds {};
    struct Status {
        RequestResponse_STATUS status;
        RequestResponse_VOLTAGE_MINMAX voltage;
        RequestResponse_SENSOR_MINMAX sensor;
        RequestResponse_MOSFET mosfet;
        RequestResponse_INFORMATION information;
        RequestResponse_FAILURE failure;
    } status {};
    struct Diagnostics {
        RequestResponse_VOLTAGES voltages;
        RequestResponse_SENSORS sensors;
        RequestResponse_BALANCES balances;
    } diagnostics {};
    struct Commands {    // unofficial
        RequestResponse_RESET reset;
        RequestResponse_MOSFET_DISCHARGE discharge;
        RequestResponse_MOSFET_CHARGE charge;
    } commands {};

    explicit Manager (const Config &conf, Connector &connector) :
        requestResponsesSpecifications ({

            { Categories::Information, Capabilities::Managing + Capabilities::Balancing,          information.config },
            { Categories::Information, Capabilities::Managing + Capabilities::Balancing,        information.hardware },
            { Categories::Information,                      Capabilities::FirmwareIndex,        information.firmware }, // No response, Managing or Balancing
            { Categories::Information, Capabilities::Managing + Capabilities::Balancing,        information.software },
            { Categories::Information, Capabilities::Managing + Capabilities::Balancing, information.battery_ratings },
            { Categories::Information, Capabilities::Managing + Capabilities::Balancing,    information.battery_code },
            { Categories::Information, Capabilities::Managing + Capabilities::Balancing,    information.battery_info },
            { Categories::Information,                           Capabilities::Managing,    information.battery_stat },
            { Categories::Information,                      Capabilities::RealTimeClock,             information.rtc },

            {  Categories::Thresholds, Capabilities::Managing + Capabilities::Balancing,          thresholds.voltage },
            {  Categories::Thresholds,                           Capabilities::Managing,          thresholds.current },
            {  Categories::Thresholds,                 Capabilities::TemperatureSensing,           thresholds.sensor }, // Balancing response, but questionable
            {  Categories::Thresholds,                           Capabilities::Managing,           thresholds.charge },
            {  Categories::Thresholds, Capabilities::Managing + Capabilities::Balancing,     thresholds.cell_voltage },
            {  Categories::Thresholds,                 Capabilities::TemperatureSensing,      thresholds.cell_sensor }, // Balancing response, but questionable
            {  Categories::Thresholds, Capabilities::Managing + Capabilities::Balancing,     thresholds.cell_balance },
            {  Categories::Thresholds, Capabilities::Managing + Capabilities::Balancing,     thresholds.shortcircuit },

            {      Categories::Status,                           Capabilities::Managing,               status.status }, // Balancing response, but voltage only
            {      Categories::Status, Capabilities::Managing + Capabilities::Balancing,              status.voltage },
            {      Categories::Status,                 Capabilities::TemperatureSensing,               status.sensor }, // Balancing response, is probably onboard sensor
            {      Categories::Status,                           Capabilities::Managing,               status.mosfet },
            {      Categories::Status, Capabilities::Managing + Capabilities::Balancing,          status.information },
            {      Categories::Status, Capabilities::Managing + Capabilities::Balancing,              status.failure },

            { Categories::Diagnostics, Capabilities::Managing + Capabilities::Balancing,        diagnostics.voltages },
            { Categories::Diagnostics,                 Capabilities::TemperatureSensing,         diagnostics.sensors },
            { Categories::Diagnostics,                          Capabilities::Balancing,        diagnostics.balances },

            {    Categories::Commands, Capabilities::Managing + Capabilities::Balancing,              commands.reset },
            {    Categories::Commands,                           Capabilities::Managing,             commands.charge },
            {    Categories::Commands,                           Capabilities::Managing,          commands.discharge }
    }),
        config (conf), connector (connector), manager (buildRequestResponses (config.capabilities)) {

        struct ResponseHandler : RequestResponseManager::Handler {
            Manager &manager;
            bool initialised = false;
            explicit ResponseHandler (Manager &i) :
                manager (i) { }
            bool handle (RequestResponse &response) override {
                if (! initialised && response.getCommand () == manager.status.information) {
                    manager.diagnostics.voltages.setCount (manager.status.information.numberOfCells);
                    manager.diagnostics.sensors.setCount (manager.status.information.numberOfSensors);
                    manager.diagnostics.balances.setCount (manager.status.information.numberOfCells);
                    initialised = true;
                    if (! manager.isEnabled (Debugging::Responses)) {
                        manager.manager.unregisterHandler (this);
                        delete this;
                    }
                }
                if (manager.isEnabled (Debugging::Responses)) {
                    DEBUG_PRINTF ("DalyBMS<%s>: response %s -- ", manager.config.id.c_str (), response.getName ());
                    response.debugDump ();    // XXX change to toString
                }
                return true;
            }
        };
        manager.registerHandler (new ResponseHandler (*this));

        struct FrameHandler : RequestResponseFrame::Receiver::Handler {
            Manager &manager;
            explicit FrameHandler (Manager &i) :
                manager (i) { }
            bool handle (RequestResponseFrame::Receiver::Handler::Type frame) {
                if (manager.isEnabled (Debugging::Frames) || (manager.isEnabled (Debugging::Errors) && frame.second == Direction::Error))
                    DEBUG_PRINTF ("DalyBMS<%s>: %s: %s\n", manager.config.id.c_str (), toString (frame.second).c_str (), frame.first.toString ().c_str ());
                if (frame.second == Direction::Receive)
                    manager.manager.receiveFrame (frame.first);
                return frame.second == Direction::Receive;
            }
        };
        connector.registerHandler (new FrameHandler (*this));
    }

    void begin () {
        DEBUG_PRINTF ("DalyBMS<%s>: begin\n", config.id.c_str ());
        connector.begin ();
    }
    void process () {
        connector.process ();
    }

    void issue (RequestResponse &request) {
        if (request.isRequestable ()) {
            if (isEnabled (Debugging::Requests))
                DEBUG_PRINTF ("DalyBMS<%s>: request %s\n", config.id.c_str (), request.getName ());
            connector.write (request.prepareRequest ());
        }
    }
    void requestInstant () {
        if (! isEnabled (Categories::Status))
            return;
        issue (status.status);
    }
    void requestStatus () {
        request (Categories::Status);
    }
    void requestDiagnostics () {
        request (Categories::Diagnostics);
    }
    void requestInitial () {
        DEBUG_PRINTF ("DalyBMS<%s>: requestInitial\n", config.id.c_str ());
        for (auto category : { Categories::Information, Categories::Thresholds })
            request (category);
    }
    void updateInitial () {
        for (auto category : { Categories::Information, Categories::Thresholds })
            update (category);
    }

protected:
    void requestInformation () {
        request (Categories::Information);
    }
    void requestThresholds () {
        request (Categories::Thresholds);
    }

public:
    void request (const Categories category) {
        if (! isEnabled (category))
            return;
        DEBUG_PRINTF ("DalyBMS<%s>: request%s\n", config.id.c_str (), toString (category).c_str ());
        auto it = requestResponses.find (category);
        if (it != requestResponses.end ())
            for (auto r : it->second)
                issue (*r);
    }
    void update (const Categories category) {
        if (! isEnabled (category))
            return;
        DEBUG_PRINTF ("DalyBMS<%s>: update%s\n", config.id.c_str (), toString (category).c_str ());
        auto it = requestResponses.find (category);
        if (it != requestResponses.end ())
            for (auto r : it->second) {
                if (! r->isValid ())    // XXX or long time?
                    issue (*r);
            }
    }

private:
    std::map<Categories, std::vector<RequestResponse *>> requestResponses;
    struct RequestResponseSpecification {
        Categories category;
        Capabilities capabilities;
        RequestResponse &request;
    };
    std::vector<RequestResponseSpecification> requestResponsesSpecifications;

    std::vector<RequestResponse *> buildRequestResponses (const Capabilities capabilities) {
        std::vector<RequestResponse *> r;
        for (const auto &item : requestResponsesSpecifications)
            if ((item.capabilities & capabilities) != Capabilities::None)
                requestResponses [item.category].push_back (&item.request);
        for (const auto &[category, requests] : requestResponses)
            r.insert (r.end (), requests.begin (), requests.end ());
        return r;
    }

    const Config &config;
    Connector &connector;
    RequestResponseManager manager;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms
