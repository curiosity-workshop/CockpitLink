#include <cockpitlink/protocol/Frame.h>
#include <cockpitlink/protocol/FrameParser.h>
#include <cockpitlink/protocol/Payloads.h>
#include <cockpitlink/protocol/TraceFormatter.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace
{
    std::vector<std::byte> bytes(
        std::string_view text)
    {
        std::vector<std::byte> result;
        result.reserve(text.size());

        for (const char character : text)
        {
            result.push_back(
                static_cast<std::byte>(character));
        }

        return result;
    }

    bool expect(
        bool condition,
        std::string_view message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            return false;
        }

        return true;
    }
}

int main()
{
    using cockpitlink::protocol::Frame;
    using cockpitlink::protocol::FrameParser;
    using cockpitlink::protocol::HelloPayload;
    using cockpitlink::protocol::MessageType;
    using cockpitlink::protocol::BehaviorRole;
    using cockpitlink::protocol::CapabilityBehaviorIds;
    using cockpitlink::protocol::CapabilityBinaryValues;
    using cockpitlink::protocol::CapabilitySerial;
    using cockpitlink::protocol::ValueType;
    using cockpitlink::protocol::decodeBehaviorAssignmentPayload;
    using cockpitlink::protocol::decodeBehaviorRequestPayload;
    using cockpitlink::protocol::decodeHelloPayload;
    using cockpitlink::protocol::decodeSubscribePayload;
    using cockpitlink::protocol::decodeValueUpdatePayload;
    using cockpitlink::protocol::encodeBehaviorAssignmentPayload;
    using cockpitlink::protocol::encodeBehaviorRequestPayload;
    using cockpitlink::protocol::encodeBoolValueUpdatePayload;
    using cockpitlink::protocol::encodeFrame;
    using cockpitlink::protocol::encodeHelloPayload;
    using cockpitlink::protocol::encodeIntValueUpdatePayload;
    using cockpitlink::protocol::encodeSubscribePayload;
    using cockpitlink::protocol::formatTraceLine;
    using cockpitlink::protocol::hasCapability;

    bool passed = true;

    const std::vector<std::byte> payload{
        std::byte{ 'A' },
        std::byte{ 0 },
        std::byte{ 'B' },
        std::byte{ 'C' }
    };

    const auto encoded =
        encodeFrame({
            MessageType::Hello,
            3,
            42,
            payload
        });

    {
        FrameParser parser;

        const auto first =
            parser.push(
                std::span<const std::byte>{
                    encoded.data(),
                    5
                });

        passed &= expect(
            first.empty(),
            "partial frame should not emit");
        passed &= expect(
            parser.hasPartialFrame(),
            "partial frame should be tracked");

        const auto second =
            parser.push(
                std::span<const std::byte>{
                    encoded.data() + 5,
                    encoded.size() - 5
                });

        passed &= expect(
            second.size() == 1,
            "completed frame should emit");
        passed &= expect(
            second[0].type == MessageType::Hello,
            "message type failed");
        passed &= expect(
            second[0].flags == 3,
            "flags failed");
        passed &= expect(
            second[0].sequence == 42,
            "sequence failed");
        passed &= expect(
            second[0].payload == payload,
            "payload failed");
        passed &= expect(
            !parser.hasPartialFrame(),
            "completed frame should clear parser state");
    }

    {
        FrameParser parser;

        auto corrupted =
            encoded;
        corrupted[10] =
            static_cast<std::byte>(
                std::to_integer<std::uint8_t>(corrupted[10]) ^ 0xff);

        const auto frames =
            parser.push(corrupted);

        passed &= expect(
            frames.empty(),
            "bad checksum should reject frame");
        passed &= expect(
            !parser.hasPartialFrame(),
            "bad checksum should clear parser state");
    }

    {
        FrameParser parser;

        const auto noise =
            bytes("noise");
        auto stream =
            noise;
        stream.insert(
            stream.end(),
            encoded.begin(),
            encoded.end());

        const auto frames =
            parser.push(stream);

        passed &= expect(
            frames.size() == 1,
            "parser should skip leading noise");
    }

    {
        const auto helloPayload =
            encodeHelloPayload({
                "Panel",
                "2026.07.20",
                512,
                2,
                3,
                CapabilitySerial |
                    CapabilityBehaviorIds |
                    CapabilityBinaryValues
            });

        const auto decoded =
            decodeHelloPayload(helloPayload);

        passed &= expect(
            decoded.has_value(),
            "hello payload should decode");
        passed &= expect(
            decoded && decoded->deviceName == "Panel",
            "hello payload device name failed");
        passed &= expect(
            decoded && decoded->firmwareVersion == "2026.07.20",
            "hello payload firmware failed");
        passed &= expect(
            decoded && decoded->maxPayload == 512,
            "hello payload max payload failed");
        passed &= expect(
            decoded && decoded->protocolMin == 2 && decoded->protocolMax == 3,
            "hello payload protocol range failed");
        passed &= expect(
            decoded && hasCapability(*decoded, CapabilityBinaryValues),
            "hello payload capabilities failed");

        const Frame helloAck{
            MessageType::HelloAck,
            0,
            7,
            helloPayload
        };

        passed &= expect(
            formatTraceLine(helloAck, false).find("device=\"Panel\"") !=
                std::string::npos,
            "hello trace should include decoded device name");
    }

    {
        const auto requestPayload =
            encodeBehaviorRequestPayload({
                3,
                BehaviorRole::Follows,
                "lights.beacon"
            });

        const auto request =
            decodeBehaviorRequestPayload(requestPayload);

        passed &= expect(
            request.has_value(),
            "behavior request should decode");
        passed &= expect(
            request && request->requestId == 3,
            "behavior request id failed");
        passed &= expect(
            request && request->role == BehaviorRole::Follows,
            "behavior request role failed");
        passed &= expect(
            request && request->behaviorId == "lights.beacon",
            "behavior request id string failed");

        const auto assignmentPayload =
            encodeBehaviorAssignmentPayload({
                3,
                42,
                ValueType::Boolean,
                CapabilityBehaviorIds
            });

        const auto assignment =
            decodeBehaviorAssignmentPayload(assignmentPayload);

        passed &= expect(
            assignment && assignment->requestId == 3 &&
            assignment->handle == 42 &&
            assignment->valueType == ValueType::Boolean,
            "behavior assignment decode failed");

        const auto subscribePayload =
            encodeSubscribePayload({
                42,
                ValueType::Boolean,
                100
            });

        const auto subscribe =
            decodeSubscribePayload(subscribePayload);

        passed &= expect(
            subscribe && subscribe->handle == 42 &&
            subscribe->rateMs == 100,
            "subscribe decode failed");

        const auto updatePayload =
            encodeBoolValueUpdatePayload(42, true);

        const auto update =
            decodeValueUpdatePayload(updatePayload);

        passed &= expect(
            update && update->handle == 42 &&
            update->valueType == ValueType::Boolean &&
            update->value.size() == 1 &&
            std::to_integer<unsigned char>(update->value[0]) == 1,
            "bool value update decode failed");

        const auto intUpdatePayload =
            encodeIntValueUpdatePayload(43, 75);

        const auto intUpdate =
            decodeValueUpdatePayload(intUpdatePayload);

        passed &= expect(
            intUpdate && intUpdate->handle == 43 &&
            intUpdate->valueType == ValueType::Int &&
            intUpdate->value.size() == 4,
            "int value update decode failed");

        const Frame intUpdateFrame{
            MessageType::ValueUpdate,
            0,
            8,
            intUpdatePayload
        };

        passed &= expect(
            formatTraceLine(intUpdateFrame, false).find("value=75") !=
                std::string::npos,
            "int value update trace should include decoded value");
    }

    return passed ? 0 : 1;
}
