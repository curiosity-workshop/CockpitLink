#pragma once

#include <cockpitlink/protocol/Frame.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cockpitlink::protocol
{
    struct HelloPayload
    {
        std::string deviceName;
        std::string firmwareVersion;
        std::uint16_t maxPayload = 0;
        std::uint8_t protocolMin = minimumProtocolVersion;
        std::uint8_t protocolMax = protocolVersion;
        std::uint16_t capabilityFlags = 0;
    };

    struct BehaviorRequestPayload
    {
        std::uint8_t requestId = 0;
        BehaviorRole role = BehaviorRole::Follows;
        std::string behaviorId;
    };

    struct BehaviorAssignmentPayload
    {
        std::uint8_t requestId = 0;
        std::uint16_t handle = 0;
        ValueType valueType = ValueType::Boolean;
        std::uint16_t capabilityFlags = 0;
    };

    struct SubscribePayload
    {
        std::uint16_t handle = 0;
        ValueType valueType = ValueType::Boolean;
        std::uint16_t rateMs = 0;
    };

    struct ValueUpdatePayload
    {
        std::uint16_t handle = 0;
        ValueType valueType = ValueType::Boolean;
        std::vector<std::byte> value;
    };

    std::vector<std::byte> encodeHelloPayload(
        const HelloPayload& payload);

    std::optional<HelloPayload> decodeHelloPayload(
        std::span<const std::byte> payload);

    std::vector<std::byte> encodeBehaviorRequestPayload(
        const BehaviorRequestPayload& payload);

    std::optional<BehaviorRequestPayload> decodeBehaviorRequestPayload(
        std::span<const std::byte> payload);

    std::vector<std::byte> encodeBehaviorAssignmentPayload(
        const BehaviorAssignmentPayload& payload);

    std::optional<BehaviorAssignmentPayload> decodeBehaviorAssignmentPayload(
        std::span<const std::byte> payload);

    std::vector<std::byte> encodeSubscribePayload(
        const SubscribePayload& payload);

    std::optional<SubscribePayload> decodeSubscribePayload(
        std::span<const std::byte> payload);

    std::vector<std::byte> encodeBoolValueUpdatePayload(
        std::uint16_t handle,
        bool value);

    std::vector<std::byte> encodeIntValueUpdatePayload(
        std::uint16_t handle,
        std::int32_t value);

    std::optional<ValueUpdatePayload> decodeValueUpdatePayload(
        std::span<const std::byte> payload);

    bool hasCapability(
        const HelloPayload& payload,
        CapabilityFlag capability);
}
