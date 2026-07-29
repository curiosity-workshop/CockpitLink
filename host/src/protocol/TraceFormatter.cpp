#include <cockpitlink/protocol/TraceFormatter.h>

#include <cockpitlink/protocol/Payloads.h>

#include <sstream>

namespace cockpitlink::protocol
{
    namespace
    {
        const char* typeName(
            MessageType type)
        {
            switch (type)
            {
            case MessageType::Hello:
                return "hello";
            case MessageType::HelloAck:
                return "hello-ack";
            case MessageType::UnsupportedVersion:
                return "unsupported-version";
            case MessageType::CapabilityError:
                return "capability-error";
            case MessageType::BehaviorRequest:
                return "behavior-request";
            case MessageType::ProfileAccepted:
                return "profile-accepted";
            case MessageType::BehaviorAssignment:
                return "behavior-assignment";
            case MessageType::BehaviorUnavailable:
                return "behavior-unavailable";
            case MessageType::Subscribe:
                return "subscribe";
            case MessageType::ValueUpdate:
                return "value-update";
            case MessageType::CommandAction:
                return "command-action";
            case MessageType::FlowControl:
                return "flow-control";
            case MessageType::Diagnostic:
                return "diagnostic";
            default:
                return "unknown";
            }
        }

        const char* roleName(
            BehaviorRole role)
        {
            switch (role)
            {
            case BehaviorRole::Follows:
                return "follows";
            case BehaviorRole::Controls:
                return "controls";
            case BehaviorRole::Triggers:
                return "triggers";
            default:
                return "unknown";
            }
        }

        const char* valueTypeName(
            ValueType valueType)
        {
            switch (valueType)
            {
            case ValueType::Boolean:
                return "bool";
            case ValueType::Int:
                return "int";
            case ValueType::Float:
                return "float";
            case ValueType::String:
                return "string";
            case ValueType::Data:
                return "data";
            case ValueType::Enum:
                return "enum";
            default:
                return "unknown";
            }
        }

        std::int32_t readI32(
            const std::vector<std::byte>& bytes)
        {
            const auto value =
                (static_cast<std::uint32_t>(
                    std::to_integer<unsigned char>(bytes[0])) << 24) |
                (static_cast<std::uint32_t>(
                    std::to_integer<unsigned char>(bytes[1])) << 16) |
                (static_cast<std::uint32_t>(
                    std::to_integer<unsigned char>(bytes[2])) << 8) |
                static_cast<std::uint32_t>(
                    std::to_integer<unsigned char>(bytes[3]));

            return static_cast<std::int32_t>(value);
        }
    }

    std::string formatTraceLine(
        const Frame& frame,
        bool transmit)
    {
        std::ostringstream line;
        line
            << (transmit ? "TX " : "RX ")
            << typeName(frame.type)
            << " seq="
            << frame.sequence
            << " bytes="
            << frame.payload.size();

        if (frame.type == MessageType::HelloAck)
        {
            const auto hello =
                decodeHelloPayload(frame.payload);

            if (hello)
            {
                line
                    << " device=\""
                    << hello->deviceName
                    << "\" firmware=\""
                    << hello->firmwareVersion
                    << "\" max-payload="
                    << hello->maxPayload
                    << " protocol="
                    << static_cast<int>(hello->protocolMin)
                    << "-"
                    << static_cast<int>(hello->protocolMax)
                    << " caps=0x"
                    << std::hex
                    << hello->capabilityFlags;
            }
        }
        else if (frame.type == MessageType::BehaviorRequest)
        {
            const auto request =
                decodeBehaviorRequestPayload(frame.payload);

            if (request)
            {
                line
                    << " request="
                    << static_cast<int>(request->requestId)
                    << " role="
                    << roleName(request->role)
                    << " behavior=\""
                    << request->behaviorId
                    << "\"";
            }
        }
        else if (frame.type == MessageType::BehaviorAssignment)
        {
            const auto assignment =
                decodeBehaviorAssignmentPayload(frame.payload);

            if (assignment)
            {
                line
                    << " request="
                    << static_cast<int>(assignment->requestId)
                    << " handle="
                    << assignment->handle
                    << " type="
                    << valueTypeName(assignment->valueType)
                    << " caps=0x"
                    << std::hex
                    << assignment->capabilityFlags;
            }
        }
        else if (frame.type == MessageType::Subscribe)
        {
            const auto subscribe =
                decodeSubscribePayload(frame.payload);

            if (subscribe)
            {
                line
                    << " handle="
                    << subscribe->handle
                    << " type="
                    << valueTypeName(subscribe->valueType)
                    << " rate-ms="
                    << subscribe->rateMs;
            }
        }
        else if (frame.type == MessageType::ValueUpdate)
        {
            const auto update =
                decodeValueUpdatePayload(frame.payload);

            if (update)
            {
                line
                    << " handle="
                    << update->handle
                    << " type="
                    << valueTypeName(update->valueType);

                if (update->valueType == ValueType::Boolean &&
                    update->value.size() == 1)
                {
                    line
                        << " value="
                        << (std::to_integer<unsigned char>(
                            update->value[0]) != 0 ? "true" : "false");
                }
                else if (update->valueType == ValueType::Int &&
                    update->value.size() == 4)
                {
                    line
                        << " value="
                        << readI32(update->value);
                }
            }
        }

        return line.str();
    }
}
