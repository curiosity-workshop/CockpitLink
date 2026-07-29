#include <cockpitlink/protocol/Payloads.h>

namespace cockpitlink::protocol
{
    namespace
    {
        void pushString(
            std::vector<std::byte>& bytes,
            std::string_view value)
        {
            for (const char character : value)
            {
                bytes.push_back(
                    static_cast<std::byte>(character));
            }

            bytes.push_back(std::byte{ 0 });
        }

        void pushU16(
            std::vector<std::byte>& bytes,
            std::uint16_t value)
        {
            bytes.push_back(
                static_cast<std::byte>((value >> 8) & 0xff));
            bytes.push_back(
                static_cast<std::byte>(value & 0xff));
        }

        void pushI32(
            std::vector<std::byte>& bytes,
            std::int32_t value)
        {
            const auto unsignedValue =
                static_cast<std::uint32_t>(value);

            bytes.push_back(
                static_cast<std::byte>((unsignedValue >> 24) & 0xff));
            bytes.push_back(
                static_cast<std::byte>((unsignedValue >> 16) & 0xff));
            bytes.push_back(
                static_cast<std::byte>((unsignedValue >> 8) & 0xff));
            bytes.push_back(
                static_cast<std::byte>(unsignedValue & 0xff));
        }

        std::optional<std::string> readNullString(
            std::span<const std::byte> payload,
            std::size_t& offset)
        {
            std::string value;

            while (offset < payload.size())
            {
                const char character =
                    static_cast<char>(
                        std::to_integer<unsigned char>(payload[offset++]));

                if (character == '\0')
                {
                    return value;
                }

                value.push_back(character);
            }

            return std::nullopt;
        }

        std::optional<std::uint16_t> readU16(
            std::span<const std::byte> payload,
            std::size_t& offset)
        {
            if (offset + 2 > payload.size())
            {
                return std::nullopt;
            }

            const auto value =
                static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(
                        std::to_integer<unsigned char>(payload[offset])) << 8) |
                    static_cast<std::uint16_t>(
                        std::to_integer<unsigned char>(payload[offset + 1])));

            offset += 2;
            return value;
        }

        bool readU8(
            std::span<const std::byte> payload,
            std::size_t& offset,
            std::uint8_t& value)
        {
            if (offset >= payload.size())
            {
                return false;
            }

            value =
                std::to_integer<unsigned char>(payload[offset++]);
            return true;
        }
    }

    std::vector<std::byte> encodeHelloPayload(
        const HelloPayload& payload)
    {
        std::vector<std::byte> bytes;

        pushString(bytes, payload.deviceName);
        pushString(bytes, payload.firmwareVersion);
        pushU16(bytes, payload.maxPayload);
        bytes.push_back(static_cast<std::byte>(payload.protocolMin));
        bytes.push_back(static_cast<std::byte>(payload.protocolMax));
        pushU16(bytes, payload.capabilityFlags);

        return bytes;
    }

    std::optional<HelloPayload> decodeHelloPayload(
        std::span<const std::byte> payload)
    {
        std::size_t offset = 0;

        auto deviceName =
            readNullString(payload, offset);
        auto firmwareVersion =
            readNullString(payload, offset);
        auto maxPayload =
            readU16(payload, offset);

        if (!deviceName || !firmwareVersion || !maxPayload ||
            offset + 4 > payload.size())
        {
            return std::nullopt;
        }

        HelloPayload result;
        result.deviceName = *deviceName;
        result.firmwareVersion = *firmwareVersion;
        result.maxPayload = *maxPayload;
        result.protocolMin =
            std::to_integer<unsigned char>(payload[offset++]);
        result.protocolMax =
            std::to_integer<unsigned char>(payload[offset++]);

        auto capabilityFlags =
            readU16(payload, offset);

        if (!capabilityFlags)
        {
            return std::nullopt;
        }

        result.capabilityFlags = *capabilityFlags;
        return result;
    }

    std::vector<std::byte> encodeBehaviorRequestPayload(
        const BehaviorRequestPayload& payload)
    {
        std::vector<std::byte> bytes;

        bytes.push_back(
            static_cast<std::byte>(payload.requestId));
        bytes.push_back(
            static_cast<std::byte>(payload.role));
        pushString(bytes, payload.behaviorId);

        return bytes;
    }

    std::optional<BehaviorRequestPayload> decodeBehaviorRequestPayload(
        std::span<const std::byte> payload)
    {
        std::size_t offset = 0;
        std::uint8_t requestId = 0;
        std::uint8_t role = 0;

        if (!readU8(payload, offset, requestId) ||
            !readU8(payload, offset, role))
        {
            return std::nullopt;
        }

        auto behaviorId =
            readNullString(payload, offset);

        if (!behaviorId)
        {
            return std::nullopt;
        }

        return BehaviorRequestPayload{
            requestId,
            static_cast<BehaviorRole>(role),
            *behaviorId
        };
    }

    std::vector<std::byte> encodeBehaviorAssignmentPayload(
        const BehaviorAssignmentPayload& payload)
    {
        std::vector<std::byte> bytes;

        bytes.push_back(
            static_cast<std::byte>(payload.requestId));
        pushU16(bytes, payload.handle);
        bytes.push_back(
            static_cast<std::byte>(payload.valueType));
        pushU16(bytes, payload.capabilityFlags);

        return bytes;
    }

    std::optional<BehaviorAssignmentPayload> decodeBehaviorAssignmentPayload(
        std::span<const std::byte> payload)
    {
        std::size_t offset = 0;
        std::uint8_t requestId = 0;
        std::uint8_t valueType = 0;

        auto readHandle =
            [&]() -> std::optional<std::uint16_t>
            {
                return readU16(payload, offset);
            };

        if (!readU8(payload, offset, requestId))
        {
            return std::nullopt;
        }

        auto handle =
            readHandle();

        if (!handle ||
            !readU8(payload, offset, valueType))
        {
            return std::nullopt;
        }

        auto capabilityFlags =
            readU16(payload, offset);

        if (!capabilityFlags)
        {
            return std::nullopt;
        }

        return BehaviorAssignmentPayload{
            requestId,
            *handle,
            static_cast<ValueType>(valueType),
            *capabilityFlags
        };
    }

    std::vector<std::byte> encodeSubscribePayload(
        const SubscribePayload& payload)
    {
        std::vector<std::byte> bytes;

        pushU16(bytes, payload.handle);
        bytes.push_back(
            static_cast<std::byte>(payload.valueType));
        pushU16(bytes, payload.rateMs);

        return bytes;
    }

    std::optional<SubscribePayload> decodeSubscribePayload(
        std::span<const std::byte> payload)
    {
        std::size_t offset = 0;

        const auto handle =
            readU16(payload, offset);

        std::uint8_t valueType = 0;

        if (!handle ||
            !readU8(payload, offset, valueType))
        {
            return std::nullopt;
        }

        const auto rateMs =
            readU16(payload, offset);

        if (!rateMs)
        {
            return std::nullopt;
        }

        return SubscribePayload{
            *handle,
            static_cast<ValueType>(valueType),
            *rateMs
        };
    }

    std::vector<std::byte> encodeBoolValueUpdatePayload(
        std::uint16_t handle,
        bool value)
    {
        std::vector<std::byte> bytes;

        pushU16(bytes, handle);
        bytes.push_back(
            static_cast<std::byte>(ValueType::Boolean));
        pushU16(bytes, 1);
        bytes.push_back(
            static_cast<std::byte>(value ? 1 : 0));

        return bytes;
    }

    std::vector<std::byte> encodeIntValueUpdatePayload(
        std::uint16_t handle,
        std::int32_t value)
    {
        std::vector<std::byte> bytes;

        pushU16(bytes, handle);
        bytes.push_back(
            static_cast<std::byte>(ValueType::Int));
        pushU16(bytes, 4);
        pushI32(bytes, value);

        return bytes;
    }

    std::optional<ValueUpdatePayload> decodeValueUpdatePayload(
        std::span<const std::byte> payload)
    {
        std::size_t offset = 0;

        const auto handle =
            readU16(payload, offset);

        std::uint8_t valueType = 0;

        if (!handle ||
            !readU8(payload, offset, valueType))
        {
            return std::nullopt;
        }

        const auto valueLength =
            readU16(payload, offset);

        if (!valueLength ||
            offset + *valueLength > payload.size())
        {
            return std::nullopt;
        }

        ValueUpdatePayload result;
        result.handle = *handle;
        result.valueType =
            static_cast<ValueType>(valueType);
        result.value.assign(
            payload.begin() + offset,
            payload.begin() + offset + *valueLength);

        return result;
    }

    bool hasCapability(
        const HelloPayload& payload,
        CapabilityFlag capability)
    {
        return (payload.capabilityFlags & capability) != 0;
    }
}
