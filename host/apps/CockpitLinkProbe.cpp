#include <cockpitlink/logging/SerialTraceLogger.h>
#include <cockpitlink/serial/SerialDeviceKind.h>
#include <cockpitlink/serial/WindowsSerialEnumerator.h>
#include <cockpitlink/serial/WindowsSerialTransport.h>
#include <cockpitlink/protocol/Frame.h>
#include <cockpitlink/protocol/FrameParser.h>
#include <cockpitlink/protocol/Payloads.h>
#include <cockpitlink/protocol/TraceFormatter.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <span>
#include <iostream>
#include <thread>
#include <vector>
#include <string_view>

namespace
{
    std::string_view kindName(
        cockpitlink::serial::SerialDeviceKind kind)
    {
        using cockpitlink::serial::SerialDeviceKind;

        switch (kind)
        {
        case SerialDeviceKind::ArduinoCompatible:
            return "arduino-compatible";
        case SerialDeviceKind::UsbSerial:
            return "usb-serial";
        case SerialDeviceKind::Bluetooth:
            return "bluetooth";
        case SerialDeviceKind::BuiltInSerial:
            return "built-in-serial";
        case SerialDeviceKind::Unknown:
        default:
            return "unknown";
        }
    }

    bool shouldProbe(
        cockpitlink::serial::SerialDeviceKind kind)
    {
        using cockpitlink::serial::SerialDeviceKind;

        return kind == SerialDeviceKind::ArduinoCompatible ||
            kind == SerialDeviceKind::UsbSerial ||
            kind == SerialDeviceKind::Unknown;
    }

    std::uint16_t behaviorHandle(
        std::string_view behaviorId)
    {
        if (behaviorId == "lights.beacon")
        {
            return 0;
        }

        if (behaviorId == "lights.nav")
        {
            return 7;
        }

        if (behaviorId == "lights.strobe")
        {
            return 8;
        }

        if (behaviorId == "engine.1.throttle")
        {
            return 1;
        }

        if (behaviorId == "engine.all.throttle")
        {
            return 9;
        }

        if (behaviorId == "engine.2.throttle")
        {
            return 2;
        }

        if (behaviorId == "engine.1.prop_rpm")
        {
            return 3;
        }

        if (behaviorId == "engine.2.prop_rpm")
        {
            return 4;
        }

        if (behaviorId == "engine.1.mixture")
        {
            return 5;
        }

        if (behaviorId == "engine.2.mixture")
        {
            return 6;
        }

        return 0xffff;
    }

    std::string_view behaviorForHandle(
        std::uint16_t handle)
    {
        switch (handle)
        {
        case 0:
            return "lights.beacon";
        case 1:
            return "engine.1.throttle";
        case 2:
            return "engine.2.throttle";
        case 3:
            return "engine.1.prop_rpm";
        case 4:
            return "engine.2.prop_rpm";
        case 5:
            return "engine.1.mixture";
        case 6:
            return "engine.2.mixture";
        case 7:
            return "lights.nav";
        case 8:
            return "lights.strobe";
        case 9:
            return "engine.all.throttle";
        default:
            return {};
        }
    }

    bool tryProbe(
        const cockpitlink::serial::SerialPortInfo& port,
        cockpitlink::logging::SerialTraceLogger& trace)
    {
        cockpitlink::serial::WindowsSerialTransport transport{
            port.portName
        };

        if (!transport.open())
        {
            std::cout
                << "  probe "
                << port.portName
                << ": open failed\n";
            return false;
        }

        cockpitlink::protocol::FrameParser parser;
        std::vector<std::byte> readBuffer(256);
        const cockpitlink::protocol::Frame helloFrame{
            cockpitlink::protocol::MessageType::Hello,
            0,
            1,
            cockpitlink::protocol::encodeHelloPayload({
                "CockpitLinkProbe",
                "host",
                static_cast<std::uint16_t>(
                    cockpitlink::protocol::maximumPayloadSize),
                cockpitlink::protocol::minimumProtocolVersion,
                cockpitlink::protocol::protocolVersion,
                cockpitlink::protocol::CapabilitySerial |
                    cockpitlink::protocol::CapabilityBehaviorIds |
                    cockpitlink::protocol::CapabilityBinaryValues |
                cockpitlink::protocol::CapabilityDecodedDiagnostics
            })
        };

        const auto deadline =
            std::chrono::steady_clock::now() +
            std::chrono::seconds{ 12 };
        bool helloSent = false;
        bool helloAckReceived = false;
        bool beaconAssigned = false;
        int throttleAssignments = 0;
        bool subscribed = false;
        bool nextBeaconValue = false;
        int updatesSent = 0;
        int throttleUpdatesReceived = 0;
        std::uint16_t beaconHandle = 0;
        std::uint16_t nextSequence = 100;
        auto nextUpdateDue =
            std::chrono::steady_clock::now();

        auto sendFrame =
            [&](const cockpitlink::protocol::Frame& frame)
            {
                const auto bytes =
                    cockpitlink::protocol::encodeFrame(frame);

                transport.write(bytes);
                trace.bytes(
                    cockpitlink::logging::TraceDirection::Transmit,
                    port.portName,
                    bytes);
                trace.frame(
                    cockpitlink::logging::TraceDirection::Transmit,
                    port.portName,
                    frame);
            };

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (!helloSent)
            {
                sendFrame(helloFrame);
                helloSent = true;
            }

            const std::size_t bytesRead =
                transport.read(readBuffer);

            if (bytesRead > 0)
            {
                trace.bytes(
                    cockpitlink::logging::TraceDirection::Receive,
                    port.portName,
                    std::span<const std::byte>{
                        readBuffer.data(),
                        bytesRead
                    });

                const auto frames =
                    parser.push(
                        std::span<const std::byte>{
                            readBuffer.data(),
                            bytesRead
                        });

                for (const auto& frame : frames)
                {
                    trace.frame(
                        cockpitlink::logging::TraceDirection::Receive,
                        port.portName,
                        frame);

                    if (frame.type !=
                        cockpitlink::protocol::MessageType::HelloAck)
                    {
                        if (frame.type ==
                            cockpitlink::protocol::MessageType::BehaviorRequest)
                        {
                            const auto request =
                                cockpitlink::protocol::decodeBehaviorRequestPayload(
                                    frame.payload);

                            if (request)
                            {
                                const auto assignedHandle =
                                    behaviorHandle(request->behaviorId);

                                if (assignedHandle == 0xffff)
                                {
                                    continue;
                                }

                                const bool isThrottle =
                                    request->behaviorId.starts_with("engine.");
                                const auto valueType =
                                    isThrottle ?
                                        cockpitlink::protocol::ValueType::Int :
                                        cockpitlink::protocol::ValueType::Boolean;

                                cockpitlink::protocol::Frame assignment{
                                    cockpitlink::protocol::MessageType::BehaviorAssignment,
                                    0,
                                    nextSequence++,
                                    cockpitlink::protocol::encodeBehaviorAssignmentPayload({
                                        request->requestId,
                                        assignedHandle,
                                        valueType,
                                        cockpitlink::protocol::CapabilityBehaviorIds |
                                            cockpitlink::protocol::CapabilityBinaryValues
                                    })
                                };

                                sendFrame(assignment);
                                if (isThrottle)
                                {
                                    ++throttleAssignments;
                                }
                                else
                                {
                                    beaconAssigned = true;
                                }

                                std::cout
                                    << "  assigned "
                                    << request->behaviorId
                                    << " -> handle "
                                    << assignedHandle
                                    << "\n";
                            }
                        }
                        else if (frame.type ==
                            cockpitlink::protocol::MessageType::Subscribe)
                        {
                            const auto subscribe =
                                cockpitlink::protocol::decodeSubscribePayload(
                                    frame.payload);

                            if (subscribe &&
                                subscribe->handle == beaconHandle)
                            {
                                subscribed = true;
                                nextUpdateDue =
                                    std::chrono::steady_clock::now();
                                std::cout
                                    << "  subscribed handle "
                                    << subscribe->handle
                                    << "\n";
                            }
                        }
                        else if (frame.type ==
                            cockpitlink::protocol::MessageType::ValueUpdate)
                        {
                            const auto update =
                                cockpitlink::protocol::decodeValueUpdatePayload(
                                    frame.payload);

                            const auto behavior =
                                update ?
                                    behaviorForHandle(update->handle) :
                                    std::string_view{};

                            if (update &&
                                !behavior.empty())
                            {
                                ++throttleUpdatesReceived;
                                std::cout
                                    << "  received "
                                    << behavior
                                    << ": "
                                    << cockpitlink::protocol::formatTraceLine(
                                        frame,
                                        false)
                                    << "\n";
                            }
                        }

                        continue;
                    }

                    helloAckReceived = true;
                    std::cout
                        << "  probe "
                        << port.portName
                        << ": "
                        << cockpitlink::protocol::formatTraceLine(
                            frame,
                            false)
                        << "\n";
                }
            }

            if (helloAckReceived &&
                beaconAssigned &&
                subscribed &&
                updatesSent < 4 &&
                std::chrono::steady_clock::now() >= nextUpdateDue)
            {
                cockpitlink::protocol::Frame update{
                    cockpitlink::protocol::MessageType::ValueUpdate,
                    0,
                    nextSequence++,
                    cockpitlink::protocol::encodeBoolValueUpdatePayload(
                        beaconHandle,
                        nextBeaconValue)
                };

                sendFrame(update);
                std::cout
                    << "  sent lights.beacon="
                    << (nextBeaconValue ? "true" : "false")
                    << "\n";

                nextBeaconValue = !nextBeaconValue;
                ++updatesSent;
                nextUpdateDue =
                    std::chrono::steady_clock::now() +
                    std::chrono::seconds{ 2 };
            }

            if (updatesSent >= 4 &&
                (throttleAssignments == 0 || throttleUpdatesReceived > 0))
            {
                return true;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds{ 50 });
        }

        if (helloAckReceived)
        {
            std::cout
                << "  probe "
                << port.portName
                << ": timed out before behavior loop completed\n";
            return beaconAssigned || throttleAssignments > 0 || subscribed;
        }

        std::cout
            << "  probe "
            << port.portName
            << ": no CockpitLink response\n";
        return false;
    }
}

int main()
{
    const std::filesystem::path tracePath =
        std::filesystem::current_path() /
        "CockpitLink" /
        "logs" /
        "CockpitLinkSerial.log";

    cockpitlink::logging::SerialTraceLogger trace{
        tracePath
    };

    trace.open();

    cockpitlink::serial::WindowsSerialEnumerator enumerator;

    const auto ports =
        enumerator.enumerate();

    std::cout
        << "CockpitLink probe\n"
        << "Serial trace: "
        << trace.path().string()
        << "\n"
        << "Ports detected: "
        << ports.size()
        << "\n";

    for (const auto& port : ports)
    {
        std::cout
            << port.portName
            << "  "
            << kindName(port.kind)
            << "  "
            << port.displayName
            << "\n";
    }

    std::cout
        << "Probing candidate ports...\n";

    for (const auto& port : ports)
    {
        if (shouldProbe(port.kind))
        {
            tryProbe(port, trace);
        }
    }

    return 0;
}
