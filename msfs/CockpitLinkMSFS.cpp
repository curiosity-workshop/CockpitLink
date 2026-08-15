#include <cockpitlink/catalog/BehaviorCatalog.h>
#include <cockpitlink/runtime/DeviceRuntime.h>

#include <Windows.h>
#include <SimConnect.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    std::atomic_bool running{ true };

    BOOL WINAPI consoleHandler(DWORD signal)
    {
        if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT)
        {
            running = false;
            return TRUE;
        }
        return FALSE;
    }

    std::int32_t readI32(std::span<const std::byte> bytes)
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

    cockpitlink::protocol::ValueType protocolValueType(
        cockpitlink::catalog::ValueType type)
    {
        using CatalogType = cockpitlink::catalog::ValueType;
        using ProtocolType = cockpitlink::protocol::ValueType;
        switch (type)
        {
        case CatalogType::Boolean: return ProtocolType::Boolean;
        case CatalogType::Int: return ProtocolType::Int;
        case CatalogType::Float: return ProtocolType::Float;
        case CatalogType::String: return ProtocolType::String;
        case CatalogType::Enum: return ProtocolType::Enum;
        default: return ProtocolType::Data;
        }
    }

    class MsfsAdapter final : public cockpitlink::runtime::ISimulatorAdapter
    {
    public:
        explicit MsfsAdapter(
            const std::vector<std::filesystem::path>& catalogLayers)
        {
            std::vector<std::string> errors;
            catalog_ = cockpitlink::catalog::loadLayeredBehaviorCatalog(
                catalogLayers, errors);
            for (const auto& error : errors)
            {
                std::cerr << "Catalog: " << error << '\n';
            }
            if (!catalog_)
            {
                return;
            }

            const auto baseSource = catalogLayers.front().string();
            for (const auto& behavior : catalog_->behaviors())
            {
                if (!behavior.msfsSource.empty() &&
                    behavior.msfsSource != baseSource)
                {
                    std::cout << "Catalog override: " << behavior.id
                        << " [msfs] <- " << behavior.msfsSource << '\n';
                }
            }

            const HRESULT result = SimConnect_Open(
                &connection_,
                "CockpitLink MSFS",
                nullptr,
                0,
                nullptr,
                0);
            if (FAILED(result))
            {
                std::cerr << "SimConnect_Open failed (HRESULT 0x"
                    << std::hex << std::uppercase
                    << static_cast<unsigned long>(result) << ").\n";
                connection_ = nullptr;
                return;
            }

            for (const auto& behavior : catalog_->behaviors())
            {
                if (!behavior.msfs || !behavior.msfs->event)
                {
                    continue;
                }
                const auto handle = catalog_->handleFor(behavior.id);
                if (!handle)
                {
                    continue;
                }
                const DWORD eventId = eventIdForHandle(*handle);
                const HRESULT mapResult = SimConnect_MapClientEventToSimEvent(
                    connection_, eventId,
                    behavior.msfs->event->event.c_str());
                if (SUCCEEDED(mapResult))
                {
                    eventIds_[*handle] = eventId;
                    rememberLastPacket("map event " + behavior.id +
                        " -> " + behavior.msfs->event->event);
                }
                else
                {
                    std::cerr << "Could not map MSFS event for "
                        << behavior.id << ".\n";
                }
            }

            SimConnect_AddToDataDefinition(connection_,
                headingSyncDefinitionId, "PLANE HEADING DEGREES GYRO",
                "Degrees", SIMCONNECT_DATATYPE_FLOAT64);
            SimConnect_EnumerateInputEvents(connection_,
                inputEventEnumerationRequestId);
        }

        ~MsfsAdapter() override
        {
            if (connection_)
            {
                SimConnect_Close(connection_);
            }
        }

        bool ready() const
        {
            return catalog_.has_value() && connection_ != nullptr;
        }

        std::optional<cockpitlink::runtime::BehaviorResolution> resolve(
            std::string_view behaviorId) const override
        {
            if (!catalog_)
            {
                return std::nullopt;
            }
            const auto* behavior = catalog_->find(behaviorId);
            const auto handle = catalog_->handleFor(behaviorId);
            if (!behavior || !handle || !behavior->msfs)
            {
                return std::nullopt;
            }
            if (!behavior->msfs->read && !behavior->msfs->write &&
                !behavior->msfs->event && !behavior->msfs->inputEvent)
            {
                return std::nullopt;
            }
            return cockpitlink::runtime::BehaviorResolution{
                *handle,
                protocolValueType(behavior->valueType),
                0
            };
        }

        void subscribe(
            const cockpitlink::protocol::SubscribePayload& subscription) override
        {
            if (!catalog_ || !connection_ ||
                subscriptions_.contains(subscription.handle))
            {
                return;
            }

            const auto* behavior = catalog_->atHandle(subscription.handle);
            if (!behavior || !behavior->msfs || !behavior->msfs->read)
            {
                return;
            }

            const auto& read = *behavior->msfs->read;
            const DWORD definitionId = definitionIdForHandle(
                subscription.handle);
            const DWORD requestId = requestIdForHandle(subscription.handle);
            if (FAILED(SimConnect_AddToDataDefinition(
                connection_, definitionId, read.simVar.c_str(),
                read.unit.c_str(), SIMCONNECT_DATATYPE_FLOAT64)))
            {
                std::cerr << "Could not define MSFS SimVar for "
                    << behavior->id << ".\n";
                return;
            }
            rememberLastPacket("define SimVar " + behavior->id +
                " -> " + read.simVar);

            const DWORD interval = std::max<DWORD>(
                1, static_cast<DWORD>(subscription.rateMs) / 16);
            if (FAILED(SimConnect_RequestDataOnSimObject(
                connection_, requestId, definitionId,
                SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME,
                SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, 0, interval, 0)))
            {
                std::cerr << "Could not subscribe to MSFS SimVar for "
                    << behavior->id << ".\n";
                return;
            }
            rememberLastPacket("subscribe " + behavior->id);
            subscriptions_[subscription.handle] = requestId;
            requestHandles_[requestId] = subscription.handle;
        }

        void writeValue(
            const cockpitlink::protocol::ValueUpdatePayload& update) override
        {
            if (!catalog_ || !connection_)
            {
                return;
            }
            const auto* behavior = catalog_->atHandle(update.handle);
            if (!behavior || !behavior->msfs)
            {
                return;
            }

            double canonical = 0.0;
            if (update.valueType == cockpitlink::protocol::ValueType::Boolean &&
                update.value.size() == 1)
            {
                canonical = std::to_integer<unsigned char>(update.value[0]) != 0;
            }
            else if (update.valueType == cockpitlink::protocol::ValueType::Int &&
                update.value.size() == 4)
            {
                canonical = readI32(update.value);
            }
            else
            {
                return;
            }

            if (behavior->msfs->inputEvent)
            {
                const auto& inputEvent = *behavior->msfs->inputEvent;
                double parameter = canonical;
                if (inputEvent.scale)
                {
                    const auto& scale = *inputEvent.scale;
                    const double clamped = std::clamp(
                        canonical, scale.fromMin, scale.fromMax);
                    const double normalized =
                        (clamped - scale.fromMin) /
                        (scale.fromMax - scale.fromMin);
                    if (inputEvent.steps)
                    {
                        const auto stepCount = *inputEvent.steps;
                        const auto step = std::min<std::uint16_t>(
                            stepCount - 1,
                            static_cast<std::uint16_t>(
                                std::floor(normalized * stepCount)));
                        const double stepped =
                            static_cast<double>(step) /
                            static_cast<double>(stepCount - 1);
                        parameter = scale.toMin +
                            stepped * (scale.toMax - scale.toMin);
                    }
                    else
                    {
                        parameter = scale.toMin +
                            normalized * (scale.toMax - scale.toMin);
                    }
                }
                const auto hash = inputEventHashes_.find(update.handle);
                if (hash != inputEventHashes_.end())
                {
                    SimConnect_SetInputEvent(connection_, hash->second,
                        sizeof(parameter), &parameter);
                }
                return;
            }
            if (!behavior->msfs->event)
            {
                return;
            }

            const auto& event = *behavior->msfs->event;
            double parameter = canonical;
            if (event.scale)
            {
                const auto& scale = *event.scale;
                const double clamped = std::clamp(
                    canonical, scale.fromMin, scale.fromMax);
                const double normalized =
                    (clamped - scale.fromMin) /
                    (scale.fromMax - scale.fromMin);
                parameter = scale.toMin +
                    normalized * (scale.toMax - scale.toMin);
            }
            transmit(update.handle,
                static_cast<DWORD>(static_cast<std::int32_t>(
                    std::lround(parameter))));
        }

        void command(
            const cockpitlink::protocol::CommandActionPayload& action) override
        {
            if (action.action ==
                cockpitlink::protocol::CommandActionKind::End)
            {
                heldTrimCommands_.erase(action.handle);
                return;
            }
            if (catalog_)
            {
                const auto* behavior = catalog_->atHandle(action.handle);
                if (behavior && behavior->id == "autopilot.heading_sync")
                {
                    pendingHeadingSyncHandle_ = action.handle;
                    SimConnect_RequestDataOnSimObject(connection_,
                        headingSyncRequestId, headingSyncDefinitionId,
                        SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE);
                    return;
                }
            }
            transmit(action.handle, 0);
            if (action.action ==
                    cockpitlink::protocol::CommandActionKind::Begin &&
                isTrimCommand(action.handle))
            {
                heldTrimCommands_[action.handle] =
                    std::chrono::steady_clock::now() + trimRepeatInterval;
            }
        }

        void tick(cockpitlink::runtime::IValuePublisher& publisher) override
        {
            if (connection_)
            {
                publisher_ = &publisher;
                SimConnect_CallDispatch(connection_, dispatch, this);
                publisher_ = nullptr;

                const auto now = std::chrono::steady_clock::now();
                for (auto& [handle, nextRepeat] : heldTrimCommands_)
                {
                    if (now >= nextRepeat)
                    {
                        transmit(handle, 0);
                        nextRepeat = now + trimRepeatInterval;
                        break;
                    }
                }
            }
        }

    private:
        static DWORD eventIdForHandle(std::uint16_t handle)
        {
            return 1000u + handle;
        }

        static DWORD definitionIdForHandle(std::uint16_t handle)
        {
            return 2000u + handle;
        }

        static DWORD requestIdForHandle(std::uint16_t handle)
        {
            return 3000u + handle;
        }

        static constexpr DWORD headingSyncDefinitionId = 9000;
        static constexpr DWORD headingSyncRequestId = 9001;
        static constexpr DWORD inputEventEnumerationRequestId = 9002;

        static void CALLBACK dispatch(
            SIMCONNECT_RECV* message,
            DWORD,
            void* context)
        {
            auto& self = *static_cast<MsfsAdapter*>(context);
            if (message->dwID == SIMCONNECT_RECV_ID_OPEN && !self.openLogged_)
            {
                const auto& opened =
                    *static_cast<SIMCONNECT_RECV_OPEN*>(message);
                std::cout << "Connected to " << opened.szApplicationName
                    << " through SimConnect.\n";
                self.openLogged_ = true;
            }
            else if (message->dwID == SIMCONNECT_RECV_ID_EXCEPTION)
            {
                const auto& error =
                    *static_cast<SIMCONNECT_RECV_EXCEPTION*>(message);
                std::cerr << "SimConnect exception " << error.dwException
                    << ", send ID " << error.dwSendID;
                if (const auto sent =
                    self.sentPacketDescriptions_.find(error.dwSendID);
                    sent != self.sentPacketDescriptions_.end())
                {
                    std::cerr << " (" << sent->second << ')';
                }
                std::cerr << ".\n";
            }
            else if (message->dwID == SIMCONNECT_RECV_ID_QUIT)
            {
                std::cout << "MSFS requested shutdown.\n";
                running = false;
            }
            else if (message->dwID == SIMCONNECT_RECV_ID_SIMOBJECT_DATA &&
                self.publisher_)
            {
                const auto& data =
                    *static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message);
                if (data.dwRequestID == headingSyncRequestId &&
                    self.pendingHeadingSyncHandle_)
                {
                    double heading = 0.0;
                    std::memcpy(&heading, &data.dwData, sizeof(heading));
                    heading = std::fmod(heading + 360.0, 360.0);
                    self.transmit(*self.pendingHeadingSyncHandle_,
                        static_cast<DWORD>(std::lround(heading)));
                    self.pendingHeadingSyncHandle_.reset();
                    return;
                }
                const auto found = self.requestHandles_.find(data.dwRequestID);
                if (found == self.requestHandles_.end())
                {
                    return;
                }

                double raw = 0.0;
                static_assert(sizeof(raw) == sizeof(std::uint64_t));
                std::memcpy(&raw, &data.dwData, sizeof(raw));
                const auto* behavior = self.catalog_->atHandle(found->second);
                if (!behavior || !behavior->msfs || !behavior->msfs->read)
                {
                    return;
                }

                double canonical = raw;
                if (const auto& scale = behavior->msfs->read->scale)
                {
                    const double clamped = std::clamp(
                        raw, scale->fromMin, scale->fromMax);
                    const double normalized =
                        (clamped - scale->fromMin) /
                        (scale->fromMax - scale->fromMin);
                    canonical = scale->toMin +
                        normalized * (scale->toMax - scale->toMin);
                }
                self.publisher_->publishInt(found->second,
                    static_cast<std::int32_t>(std::lround(canonical)));
            }
            else if (message->dwID ==
                SIMCONNECT_RECV_ID_ENUMERATE_INPUT_EVENTS)
            {
                const auto& events =
                    *static_cast<SIMCONNECT_RECV_ENUMERATE_INPUT_EVENTS*>(
                        message);
                for (DWORD index = 0; index < events.dwArraySize; ++index)
                {
                    const auto& descriptor = events.rgData[index];
                    for (const auto& behavior : self.catalog_->behaviors())
                    {
                        if (!behavior.msfs ||
                            !behavior.msfs->inputEvent ||
                            behavior.msfs->inputEvent->name != descriptor.Name)
                        {
                            continue;
                        }
                        if (const auto handle =
                            self.catalog_->handleFor(behavior.id))
                        {
                            self.inputEventHashes_[*handle] = descriptor.Hash;
                            std::cout << "Resolved Input Event: "
                                << behavior.id << " -> "
                                << descriptor.Name << ".\n";
                        }
                    }
                }
            }
        }

        void transmit(std::uint16_t handle, DWORD parameter)
        {
            const auto found = eventIds_.find(handle);
            if (found == eventIds_.end() || !connection_)
            {
                return;
            }
            SimConnect_TransmitClientEvent(
                connection_,
                SIMCONNECT_OBJECT_ID_USER,
                found->second,
                parameter,
                SIMCONNECT_GROUP_PRIORITY_HIGHEST,
                SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        }

        bool isTrimCommand(std::uint16_t handle) const
        {
            if (!catalog_)
            {
                return false;
            }
            const auto* behavior = catalog_->atHandle(handle);
            return behavior &&
                (behavior->id == "flight.elevator_trim_up" ||
                 behavior->id == "flight.elevator_trim_down" ||
                 behavior->id == "flight.rudder_trim_left" ||
                 behavior->id == "flight.rudder_trim_right");
        }

        void rememberLastPacket(std::string description)
        {
            DWORD packetId = 0;
            if (connection_ && SUCCEEDED(SimConnect_GetLastSentPacketID(
                connection_, &packetId)))
            {
                sentPacketDescriptions_[packetId] = std::move(description);
            }
        }

        std::optional<cockpitlink::catalog::BehaviorCatalog> catalog_;
        HANDLE connection_ = nullptr;
        std::unordered_map<std::uint16_t, DWORD> eventIds_;
        std::unordered_map<std::uint16_t, DWORD> subscriptions_;
        std::unordered_map<DWORD, std::uint16_t> requestHandles_;
        std::unordered_map<DWORD, std::string> sentPacketDescriptions_;
        std::unordered_map<std::uint16_t, UINT64> inputEventHashes_;
        cockpitlink::runtime::IValuePublisher* publisher_ = nullptr;
        std::optional<std::uint16_t> pendingHeadingSyncHandle_;
        std::unordered_map<std::uint16_t,
            std::chrono::steady_clock::time_point> heldTrimCommands_;
        static constexpr auto trimRepeatInterval =
            std::chrono::milliseconds{ 150 };
        bool openLogged_ = false;
    };
}

int main(int argc, char** argv)
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::vector<std::filesystem::path> catalogLayers;
    if (argc > 1)
    {
        for (int index = 1; index < argc; ++index)
        {
            catalogLayers.emplace_back(argv[index]);
        }
    }
    else
    {
        catalogLayers.emplace_back("catalog/base-behaviors.json");
    }

    const std::filesystem::path localProfileDirectory{ "profiles/local" };
    std::vector<std::filesystem::path> localProfiles;
    std::error_code directoryError;
    if (std::filesystem::is_directory(
        localProfileDirectory, directoryError))
    {
        for (const auto& entry : std::filesystem::directory_iterator(
            localProfileDirectory, directoryError))
        {
            if (directoryError)
            {
                break;
            }
            if (entry.is_regular_file() &&
                entry.path().extension() == ".json")
            {
                localProfiles.push_back(entry.path());
            }
        }
    }
    std::sort(localProfiles.begin(), localProfiles.end(),
        [](const auto& left, const auto& right)
        {
            return left.generic_string() < right.generic_string();
        });
    for (const auto& localProfile : localProfiles)
    {
        const auto duplicate = std::find_if(
            catalogLayers.begin(), catalogLayers.end(),
            [&](const auto& existing)
            {
                return existing.lexically_normal() ==
                    localProfile.lexically_normal();
            });
        if (duplicate == catalogLayers.end())
        {
            catalogLayers.push_back(localProfile);
        }
    }

    std::cout << "Catalog layers:\n";
    for (const auto& layer : catalogLayers)
    {
        std::cout << "  " << layer.string() << '\n';
    }

    SetConsoleCtrlHandler(consoleHandler, TRUE);
    MsfsAdapter adapter{ catalogLayers };
    if (!adapter.ready())
    {
        return 1;
    }

    cockpitlink::runtime::DeviceRuntime runtime{
        adapter,
        "CockpitLinkMSFS",
        [](std::string_view message)
        {
            std::cout << message << '\n';
        }
    };

    std::cout << "CockpitLink MSFS is running. Press Ctrl+C to stop.\n";
    while (running)
    {
        runtime.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
    }
    return 0;
}
