
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
    virtual void begin () = 0;
    virtual void end () = 0;
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
    Conditions = 1 << 2,
    Diagnostics = 1 << 3,
    Commands = 1 << 4,
    All = Information | Thresholds | Conditions | Diagnostics | Commands
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
    case Categories::Conditions :
        return "Conditions";
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
                        ALWAYS_DEBUG_PRINTF ("RequestResponseManager<%s>: frame complete but not valid\n", _id.c_str ());
                }
            else {
                if (it->second->isComplete ())
                    ALWAYS_DEBUG_PRINTF ("RequestResponseManager<%s>: frame complete but unprocessable\n", _id.c_str ());
            }
        else {
            ALWAYS_DEBUG_PRINTF ("RequestResponseManager<%s>: frame handler not found, command=0x%02X\n", _id.c_str (), frame.getCommand ());
        }
        return false;
    }

    explicit RequestResponseManager (const String &id, const std::vector<RequestResponse *> &requests) :
        _id (id),
        _requests (requests) {
        for (auto &request : _requests)
            _requestsMap [request->getCommand ()] = request;
    }

    const String _id;
    const std::vector<RequestResponse *> _requests {};
    std::map<uint8_t, RequestResponse *> _requestsMap {};
};

// -----------------------------------------------------------------------------------------------

class Manager;
void dumpDebug (const Manager &);

class Manager {

public:
    using Capabilities = daly_bms::Capabilities;
    using Categories = daly_bms::Categories;
    using Debugging = daly_bms::Debugging;

    struct Config {
        String id;
        Capabilities capabilities { Capabilities::None };
        Categories categories { Categories::All };
        Debugging debugging { Debugging::Errors };
    };

    struct Status {
        ActivationTracker received;
        ActivationTracker badframes;
    };

    const Config &getConfig () const {
        return config;
    }
    const Status &getStatus () const {
        return status;
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
    struct Conditions {
        RequestResponse_STATUS status;
        RequestResponse_VOLTAGE_MINMAX voltage;
        RequestResponse_SENSOR_MINMAX sensor;
        RequestResponse_MOSFET mosfet;
        RequestResponse_INFORMATION information;
        RequestResponse_FAILURE failure;
    } conditions {};
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

            {  Categories::Conditions,                           Capabilities::Managing,           conditions.status }, // Balancing response, but voltage only
            {  Categories::Conditions, Capabilities::Managing + Capabilities::Balancing,          conditions.voltage },
            {  Categories::Conditions,                 Capabilities::TemperatureSensing,           conditions.sensor }, // Balancing response, is probably onboard sensor
            {  Categories::Conditions,                           Capabilities::Managing,           conditions.mosfet },
            {  Categories::Conditions, Capabilities::Managing + Capabilities::Balancing,      conditions.information },
            {  Categories::Conditions, Capabilities::Managing + Capabilities::Balancing,          conditions.failure },

            { Categories::Diagnostics, Capabilities::Managing + Capabilities::Balancing,        diagnostics.voltages },
            { Categories::Diagnostics,                 Capabilities::TemperatureSensing,         diagnostics.sensors },
            { Categories::Diagnostics,                          Capabilities::Balancing,        diagnostics.balances },

            {    Categories::Commands, Capabilities::Managing + Capabilities::Balancing,              commands.reset },
            {    Categories::Commands,                           Capabilities::Managing,             commands.charge },
            {    Categories::Commands,                           Capabilities::Managing,          commands.discharge }
    }),
        config (conf), connector (connector), manager (config.id, buildRequestResponses (config.capabilities)) {

        struct ResponseHandler : RequestResponseManager::Handler {
            Manager &manager;
            bool initialised = false;
            explicit ResponseHandler (Manager &i) :
                manager (i) { }
            bool handle (RequestResponse &response) override {
                manager.status.received++;
                if (! initialised && response.getCommand () == manager.conditions.information) {
                    manager.diagnostics.voltages.setCount (manager.conditions.information.numberOfCells);
                    manager.diagnostics.sensors.setCount (manager.conditions.information.numberOfSensors);
                    manager.diagnostics.balances.setCount (manager.conditions.information.numberOfCells);
                    initialised = true;
                    // if (! manager.isEnabled (Debugging::Responses)) {
                    //     manager.manager.unregisterHandler (this);
                    //     delete this;
                    // }
                }
                if (manager.isEnabled (Debugging::Responses)) {
                    ALWAYS_DEBUG_PRINTF ("DalyBMS<%s>: response %s -- ", manager.config.id.c_str (), response.getName ());
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
                    ALWAYS_DEBUG_PRINTF ("DalyBMS<%s>: %s: %s\n", manager.config.id.c_str (), toString (frame.second).c_str (), frame.first.toString ().c_str ());
                if (frame.second == Direction::Error)
                    manager.status.badframes ++;
                if (frame.second == Direction::Receive)
                    manager.manager.receiveFrame (frame.first);
                return frame.second == Direction::Receive;
            }
        };
        connector.registerHandler (new FrameHandler (*this));
    }
    friend RequestResponseFrame::Receiver::Handler;

    void begin () {
        connector.begin ();
    }
    void end () {
        connector.end ();
    }
    void process () {
        connector.process ();
    }

    template <uint8_t COMMAND>
    void command (RequestResponse_TYPE_ONOFF<COMMAND> &request, typename RequestResponse_TYPE_ONOFF<COMMAND>::Setting setting) {
        if (isEnabled (Categories::Commands) && isEnabled (&request) && request.isRequestable ()) {
            if (isEnabled (Debugging::Requests))
                ALWAYS_DEBUG_PRINTF ("DalyBMS<%s>: command %s\n", config.id.c_str (), request.getName ());
            connector.write (request.prepareRequest (setting));
        }
    }

    void issue (RequestResponse &request) {
        if (request.isRequestable ()) {
            if (isEnabled (Debugging::Requests))
                ALWAYS_DEBUG_PRINTF ("DalyBMS<%s>: request %s\n", config.id.c_str (), request.getName ());
            connector.write (request.prepareRequest ());
        }
    }
    void requestInstant () {
        if (isEnabled (Categories::Conditions)) {
            if (isEnabled (&conditions.status))
                issue (conditions.status);
            if (isEnabled (&conditions.mosfet))
                issue (conditions.mosfet);
            if (isEnabled (&conditions.failure))
                issue (conditions.failure);
        }
    }
    void requestConditions () {
        request (Categories::Conditions);
    }
    void requestDiagnostics () {
        request (Categories::Diagnostics);
    }
    void requestInitial () {
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
        DALYBMS_DEBUG_PRINTF ("DalyBMS<%s>: request%s\n", config.id.c_str (), toString (category).c_str ());
        auto it = requestResponses.find (category);
        if (it != requestResponses.end ())
            for (auto r : it->second)
                issue (*r);
    }
    void update (const Categories category) {
        if (! isEnabled (category))
            return;
        DALYBMS_DEBUG_PRINTF ("DalyBMS<%s>: update%s\n", config.id.c_str (), toString (category).c_str ());
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
    Status status;
    Connector &connector;
    RequestResponseManager manager;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

}    // namespace daly_bms
