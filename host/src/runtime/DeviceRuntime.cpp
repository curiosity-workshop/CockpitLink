#include <cockpitlink/runtime/DeviceRuntime.h>

#include <cockpitlink/protocol/Payloads.h>
#include <cockpitlink/serial/SerialDeviceKind.h>
#include <cockpitlink/serial/WindowsSerialTransport.h>
#include <cockpitlink/transport/TransportSession.h>

#include <chrono>
#include <sstream>
#include <utility>

namespace cockpitlink::runtime
{
    namespace
    {
        constexpr auto baudRate = 115200u;
        constexpr auto settleDelay = std::chrono::seconds{ 3 };
        constexpr auto probeInterval = std::chrono::milliseconds{ 250 };
        constexpr auto probeTimeout = std::chrono::seconds{ 6 };
        constexpr auto assignmentInterval = std::chrono::milliseconds{ 100 };

        bool shouldProbe(serial::SerialDeviceKind kind)
        {
            using serial::SerialDeviceKind;
            return kind == SerialDeviceKind::ArduinoCompatible ||
                kind == SerialDeviceKind::UsbSerial ||
                kind == SerialDeviceKind::Unknown;
        }
    }

    DeviceRuntime::DeviceRuntime(
        ISimulatorAdapter& adapter,
        std::string hostName,
        Logger logger)
        : adapter_(adapter),
          hostName_(std::move(hostName)),
          logger_(std::move(logger))
    {
        reconnect();
    }

    DeviceRuntime::~DeviceRuntime()
    {
        disconnect();
    }

    void DeviceRuntime::reconnect()
    {
        disconnect();
        ports_.clear();
        portIndex_ = 0;
        nextActionAt_ = std::chrono::steady_clock::now();

        for (const auto& port : enumerator_.enumerate())
        {
            if (shouldProbe(port.kind))
            {
                ports_.push_back(port.portName);
            }
        }

        std::ostringstream message;
        message << "CockpitLink: found " << ports_.size()
            << " candidate serial port(s).";
        log(message.str());
    }

    void DeviceRuntime::disconnect()
    {
        closeCurrentPort();
        ports_.clear();
        portIndex_ = 0;
    }

    void DeviceRuntime::closeCurrentPort()
    {
        if (transport_)
        {
            transport_->close();
        }
        session_.reset();
        transport_.reset();
        connectedPort_.clear();
        state_ = ConnectionState::Idle;
        helloAckReceived_ = false;
        pendingAssignments_.clear();
        nextAssignmentAt_ = {};
    }

    void DeviceRuntime::tick()
    {
        adapter_.tick(*this);
        const auto now = std::chrono::steady_clock::now();

        switch (state_)
        {
        case ConnectionState::Idle:
            startNextPort(now);
            break;
        case ConnectionState::WaitingForSettle:
            if (now >= nextActionAt_)
            {
                state_ = ConnectionState::Probing;
                nextActionAt_ = now;
            }
            break;
        case ConnectionState::Probing:
            tickProbe(now);
            break;
        case ConnectionState::Connected:
            readPort();
            break;
        }

        sendNextAssignment(now);
    }

    bool DeviceRuntime::connected() const
    {
        return state_ == ConnectionState::Connected;
    }

    const std::string& DeviceRuntime::connectedPort() const
    {
        return connectedPort_;
    }

    void DeviceRuntime::startNextPort(
        std::chrono::steady_clock::time_point now)
    {
        if (transport_ || portIndex_ >= ports_.size())
        {
            return;
        }

        const std::string portName = ports_[portIndex_++];
        transport_ = std::make_unique<serial::WindowsSerialTransport>(
            portName,
            baudRate,
            serial::WindowsSerialControlMode::DtrRtsDisabled);

        if (!transport_->open())
        {
            log("CockpitLink: could not open " + portName + ".");
            transport_.reset();
            return;
        }

        session_ = std::make_unique<transport::TransportSession>(
            *transport_,
            transport::TransportSessionOptions{
                .readBufferSize = 256,
                .maximumReadPasses = 4,
                .maximumMessagesPerTick = 16,
                .maximumWriteBytesPerTick = 64
            });
        connectedPort_ = portName;
        state_ = ConnectionState::WaitingForSettle;
        nextActionAt_ = now + settleDelay;
        portDeadline_ = now + probeTimeout;
        log("CockpitLink: probing " + portName + ".");
    }

    void DeviceRuntime::tickProbe(
        std::chrono::steady_clock::time_point now)
    {
        if (now >= nextActionAt_)
        {
            sendHello();
            nextActionAt_ = now + probeInterval;
        }

        readPort();
        if (helloAckReceived_)
        {
            state_ = ConnectionState::Connected;
            log("CockpitLink: device connected on " + connectedPort_ + ".");
        }
        else if (now >= portDeadline_)
        {
            log("CockpitLink: no device response on " + connectedPort_ + ".");
            closeCurrentPort();
        }
    }

    void DeviceRuntime::readPort()
    {
        if (!session_ || !transport_ || !transport_->isOpen())
        {
            return;
        }

        const auto result = session_->tick();
        for (const auto& frame : result.frames)
        {
            handleFrame(frame);
        }
    }

    void DeviceRuntime::handleFrame(const protocol::Frame& frame)
    {
        using protocol::MessageType;
        switch (frame.type)
        {
        case MessageType::HelloAck:
            if (const auto hello = protocol::decodeHelloPayload(frame.payload))
            {
                helloAckReceived_ = true;
                log("CockpitLink: identified " + hello->deviceName +
                    " firmware " + hello->firmwareVersion + ".");
            }
            break;
        case MessageType::BehaviorRequest:
            if (const auto request =
                protocol::decodeBehaviorRequestPayload(frame.payload))
            {
                if (const auto resolution = adapter_.resolve(request->behaviorId))
                {
                    pendingAssignments_.push_back({
                        request->requestId,
                        *resolution
                    });
                }
                else
                {
                    log("CockpitLink: unsupported behavior " +
                        request->behaviorId + ".");
                }
            }
            break;
        case MessageType::Subscribe:
            if (const auto subscription =
                protocol::decodeSubscribePayload(frame.payload))
            {
                adapter_.subscribe(*subscription);
            }
            break;
        case MessageType::ValueUpdate:
            if (const auto update =
                protocol::decodeValueUpdatePayload(frame.payload))
            {
                adapter_.writeValue(*update);
            }
            break;
        case MessageType::CommandAction:
            if (const auto action =
                protocol::decodeCommandActionPayload(frame.payload))
            {
                adapter_.command(*action);
            }
            break;
        default:
            break;
        }
    }

    void DeviceRuntime::sendHello()
    {
        sendReliable({
            protocol::MessageType::Hello,
            0,
            nextSequence_++,
            protocol::encodeHelloPayload({
                hostName_,
                "msfs",
                static_cast<std::uint16_t>(protocol::maximumPayloadSize),
                protocol::minimumProtocolVersion,
                protocol::protocolVersion,
                protocol::CapabilitySerial |
                    protocol::CapabilityBehaviorIds |
                    protocol::CapabilityBinaryValues |
                    protocol::CapabilityDecodedDiagnostics
            })
        });
    }

    void DeviceRuntime::sendNextAssignment(
        std::chrono::steady_clock::time_point now)
    {
        if (pendingAssignments_.empty() || now < nextAssignmentAt_)
        {
            return;
        }

        const auto assignment = pendingAssignments_.front();
        pendingAssignments_.pop_front();
        sendReliable({
            protocol::MessageType::BehaviorAssignment,
            0,
            nextSequence_++,
            protocol::encodeBehaviorAssignmentPayload({
                assignment.requestId,
                assignment.resolution.handle,
                assignment.resolution.valueType,
                assignment.resolution.capabilityFlags
            })
        });
        nextAssignmentAt_ = now + assignmentInterval;
    }

    void DeviceRuntime::publishBool(std::uint16_t handle, bool value)
    {
        sendLatest(handle, {
            protocol::MessageType::ValueUpdate,
            0,
            nextSequence_++,
            protocol::encodeBoolValueUpdatePayload(handle, value)
        });
    }

    void DeviceRuntime::publishInt(
        std::uint16_t handle,
        std::int32_t value)
    {
        sendLatest(handle, {
            protocol::MessageType::ValueUpdate,
            0,
            nextSequence_++,
            protocol::encodeIntValueUpdatePayload(handle, value)
        });
    }

    void DeviceRuntime::sendReliable(protocol::Frame frame)
    {
        if (session_ && transport_ && transport_->isOpen())
        {
            session_->queueReliable(frame);
        }
    }

    void DeviceRuntime::sendLatest(
        std::uint16_t handle,
        protocol::Frame frame)
    {
        if (session_ && transport_ && transport_->isOpen())
        {
            session_->queueLatest(handle, frame);
        }
    }

    void DeviceRuntime::log(std::string message) const
    {
        if (logger_)
        {
            logger_(message);
        }
    }
}
