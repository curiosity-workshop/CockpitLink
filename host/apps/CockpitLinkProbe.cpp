#include <cockpitlink/catalog/BehaviorCatalog.h>
#include <cockpitlink/logging/SerialTraceLogger.h>
#include <cockpitlink/serial/SerialDeviceKind.h>
#include <cockpitlink/serial/WindowsSerialEnumerator.h>
#include <cockpitlink/serial/WindowsSerialTransport.h>
#include <cockpitlink/protocol/Frame.h>
#include <cockpitlink/protocol/FrameParser.h>
#include <cockpitlink/protocol/Payloads.h>
#include <cockpitlink/protocol/TraceFormatter.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <span>
#include <iostream>
#include <optional>
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

    std::optional<cockpitlink::protocol::ValueType> protocolValueType(
        cockpitlink::catalog::ValueType valueType)
    {
        using CatalogType = cockpitlink::catalog::ValueType;
        using ProtocolType = cockpitlink::protocol::ValueType;

        switch (valueType)
        {
        case CatalogType::Boolean:
            return ProtocolType::Boolean;
        case CatalogType::Int:
            return ProtocolType::Int;
        case CatalogType::Float:
            return ProtocolType::Float;
        case CatalogType::String:
            return ProtocolType::String;
        case CatalogType::Data:
            return ProtocolType::Data;
        case CatalogType::Enum:
            return ProtocolType::Enum;
        }

        return std::nullopt;
    }

    std::optional<std::filesystem::path> findCatalogPath()
    {
        const std::array candidates{
            std::filesystem::current_path() /
                "catalog" / "base-behaviors.json",
            std::filesystem::current_path() /
                "CockpitLink" / "catalog" / "base-behaviors.json"
        };

        for (const auto& candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
            {
                return candidate;
            }
        }

        return std::nullopt;
    }

    bool tryProbe(
        const cockpitlink::serial::SerialPortInfo& port,
        cockpitlink::logging::SerialTraceLogger& trace,
        const cockpitlink::catalog::BehaviorCatalog& catalog)
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
                                    catalog.handleFor(request->behaviorId);
                                const auto* behavior =
                                    catalog.find(request->behaviorId);
                                const auto assignedType =
                                    behavior ?
                                        protocolValueType(
                                            behavior->valueType) :
                                        std::nullopt;

                                if (!assignedHandle ||
                                    !behavior ||
                                    !behavior->xplane ||
                                    !assignedType)
                                {
                                    continue;
                                }

                                const bool isThrottle =
                                    request->behaviorId.starts_with("engine.");

                                cockpitlink::protocol::Frame assignment{
                                    cockpitlink::protocol::MessageType::BehaviorAssignment,
                                    0,
                                    nextSequence++,
                                    cockpitlink::protocol::encodeBehaviorAssignmentPayload({
                                        request->requestId,
                                        *assignedHandle,
                                        *assignedType,
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
                                    beaconHandle = *assignedHandle;
                                }

                                std::cout
                                    << "  assigned "
                                    << request->behaviorId
                                    << " -> handle "
                                    << *assignedHandle
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

                            const auto* behavior =
                                update ?
                                    catalog.atHandle(update->handle) :
                                    nullptr;

                            if (update &&
                                behavior)
                            {
                                ++throttleUpdatesReceived;
                                std::cout
                                    << "  received "
                                    << behavior->id
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
    const auto catalogPath = findCatalogPath();

    if (!catalogPath)
    {
        std::cerr
            << "CockpitLink catalog not found. Run the probe from the "
               "repository root or its parent.\n";
        return 1;
    }

    std::vector<std::string> catalogErrors;
    const auto catalog =
        cockpitlink::catalog::loadBehaviorCatalog(
            *catalogPath,
            catalogErrors);

    if (!catalog)
    {
        std::cerr
            << "CockpitLink catalog could not be loaded:\n";

        for (const auto& error : catalogErrors)
        {
            std::cerr << "  " << error << '\n';
        }

        return 1;
    }

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
        << "Catalog: "
        << catalog->name()
        << " ("
        << catalog->behaviors().size()
        << " behaviors)\n"
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
        tryProbe(port, trace, *catalog);
        }
    }

    return 0;
}
