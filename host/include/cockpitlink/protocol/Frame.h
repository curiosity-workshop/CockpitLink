#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace cockpitlink::protocol
{
    constexpr std::byte magic0{ 'C' };
    constexpr std::byte magic1{ 'L' };
    constexpr std::uint8_t minimumProtocolVersion = 2;
    constexpr std::uint8_t protocolVersion = 2;
    constexpr std::size_t headerSize = 9;
    constexpr std::size_t checksumSize = 4;
    constexpr std::size_t maximumPayloadSize = 4096;

    enum class MessageType : std::uint8_t
    {
        Hello = 1,
        HelloAck = 2,
        UnsupportedVersion = 3,
        CapabilityError = 4,
        BehaviorRequest = 10,
        ProfileAccepted = 11,
        BehaviorAssignment = 12,
        BehaviorUnavailable = 13,
        Subscribe = 20,
        ValueUpdate = 21,
        CommandAction = 30,
        FlowControl = 40,
        Diagnostic = 50
    };

    enum CapabilityFlag : std::uint16_t
    {
        CapabilitySerial = 1u << 0,
        CapabilityTcp = 1u << 1,
        CapabilityUdpDiscovery = 1u << 2,
        CapabilityBehaviorIds = 1u << 3,
        CapabilityBinaryValues = 1u << 4,
        CapabilityDecodedDiagnostics = 1u << 5,
        CapabilityAckRequested = 1u << 6
    };

    enum class ValueType : std::uint8_t
    {
        Boolean = 1,
        Int = 2,
        Float = 3,
        String = 4,
        Data = 5,
        Enum = 6
    };

    enum class BehaviorRole : std::uint8_t
    {
        Follows = 1,
        Controls = 2,
        Triggers = 3
    };

    enum class FailureCode : std::uint8_t
    {
        UnsupportedProtocolVersion = 1,
        BehaviorUnknown = 2,
        CapabilityUnavailable = 3,
        PayloadTooLarge = 4,
        Unauthorized = 5
    };

    struct Frame
    {
        MessageType type = MessageType::Diagnostic;
        std::uint8_t flags = 0;
        std::uint16_t sequence = 0;
        std::vector<std::byte> payload;
    };

    std::vector<std::byte> encodeFrame(
        const Frame& frame);

    std::uint32_t crc32(
        std::span<const std::byte> bytes);
}
