#pragma once

#include <cockpitlink/protocol/Frame.h>
#include <cockpitlink/protocol/Payloads.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace cockpitlink::runtime
{
    struct BehaviorResolution
    {
        std::uint16_t handle = 0;
        protocol::ValueType valueType = protocol::ValueType::Boolean;
        std::uint16_t capabilityFlags = 0;
    };

    class IValuePublisher
    {
    public:
        virtual ~IValuePublisher() = default;
        virtual void publishBool(std::uint16_t handle, bool value) = 0;
        virtual void publishInt(std::uint16_t handle, std::int32_t value) = 0;
    };

    class ISimulatorAdapter
    {
    public:
        virtual ~ISimulatorAdapter() = default;

        virtual std::optional<BehaviorResolution> resolve(
            std::string_view behaviorId) const = 0;
        virtual void subscribe(
            const protocol::SubscribePayload& subscription) = 0;
        virtual void writeValue(
            const protocol::ValueUpdatePayload& update) = 0;
        virtual void command(
            const protocol::CommandActionPayload& action) = 0;
        virtual void tick(IValuePublisher& publisher) = 0;
    };
}
