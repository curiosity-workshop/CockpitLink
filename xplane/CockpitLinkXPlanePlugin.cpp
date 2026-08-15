#include <cockpitlink/catalog/BehaviorCatalog.h>
#include <cockpitlink/protocol/Frame.h>
#include <cockpitlink/protocol/FrameParser.h>
#include <cockpitlink/protocol/Payloads.h>
#include <cockpitlink/serial/SerialDeviceKind.h>
#include <cockpitlink/serial/WindowsSerialEnumerator.h>
#include <cockpitlink/serial/WindowsSerialTransport.h>
#include <cockpitlink/transport/TransportSession.h>

#include <XPLMDataAccess.h>
#include <XPLMMenus.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    enum class MenuAction : std::intptr_t
    {
        Reconnect = 1,
        Disconnect = 2
    };

    enum class ConnectionState
    {
        Idle,
        WaitingForSettle,
        Probing,
        Connected
    };

    struct BoolBinding
    {
        std::string behaviorId;
        std::uint16_t handle = 0;
        std::string readDataRef;
        std::string writeDataRef;
        cockpitlink::catalog::WriteStrategy writeStrategy =
            cockpitlink::catalog::WriteStrategy::Unsupported;
        std::string toggleCommand;
    };

    struct PercentAxisBinding
    {
        std::string behaviorId;
        std::uint16_t handle = 0;
        std::string dataRef;
        int xplaneElement = 0;
        int xplaneElementCount = 1;
        int inputMinimum = 0;
        int inputMaximum = 100;
        float outputMinimum = 0.0f;
        float outputMaximum = 1.0f;
        bool xplaneArray = true;
    };

    struct CommandBinding
    {
        std::string behaviorId;
        std::uint16_t handle = 0;
        std::string command;
    };

    struct IntReadBinding
    {
        std::string behaviorId;
        std::uint16_t handle = 0;
        std::string dataRef;
        int element = 0;
        bool xplaneArray = false;
        double inputMinimum = 0.0;
        double inputMaximum = 1.0;
        double outputMinimum = 0.0;
        double outputMaximum = 1.0;
    };

    std::vector<BoolBinding> boolBindings;
    std::vector<PercentAxisBinding> percentAxisBindings;
    std::vector<CommandBinding> commandBindings;
    std::vector<IntReadBinding> intReadBindings;
    constexpr std::size_t maxBehaviorHandles = 256;
    constexpr auto baudRate = 115200u;
    constexpr auto openSettleDelay =
        std::chrono::seconds{ 3 };
    constexpr auto probeRetryInterval =
        std::chrono::milliseconds{ 250 };
    constexpr auto portTimeout =
        std::chrono::seconds{ 6 };
    constexpr auto assignmentInterval =
        std::chrono::milliseconds{ 100 };

    void debugLog(
        std::string_view message)
    {
        const std::string stableMessage{ message };
        XPLMDebugString(stableMessage.c_str());
    }

    bool loadCatalogBindings()
    {
        std::array<char, 2048> pluginPath{};
        XPLMGetPluginInfo(
            XPLMGetMyID(),
            nullptr,
            pluginPath.data(),
            nullptr,
            nullptr);

        const std::array candidates{
            std::filesystem::path{ pluginPath.data() }.
                parent_path() / "catalog" / "base-behaviors.json",
            std::filesystem::current_path() /
                "catalog" / "base-behaviors.json",
            std::filesystem::current_path() /
                "CockpitLink" / "catalog" / "base-behaviors.json"
        };

        std::optional<cockpitlink::catalog::BehaviorCatalog> catalog;
        std::vector<std::string> errors;
        std::filesystem::path loadedPath;

        for (const auto& candidate : candidates)
        {
            if (!std::filesystem::exists(candidate))
            {
                continue;
            }

            catalog =
                cockpitlink::catalog::loadBehaviorCatalog(
                    candidate,
                    errors);

            if (catalog)
            {
                loadedPath = candidate;
                break;
            }
        }

        if (!catalog)
        {
            debugLog(
                "CockpitLink: behavior catalog could not be loaded.\n");

            for (const auto& error : errors)
            {
                debugLog("CockpitLink: catalog: " + error + "\n");
            }

            return false;
        }

        boolBindings.clear();
        percentAxisBindings.clear();
        commandBindings.clear();
        intReadBindings.clear();

        for (const auto& behavior : catalog->behaviors())
        {
            const auto handle = catalog->handleFor(behavior.id);

            if (!handle ||
                *handle >= maxBehaviorHandles ||
                !behavior.xplane)
            {
                continue;
            }

            const auto& xplane = *behavior.xplane;

            if (xplane.command)
            {
                commandBindings.push_back({
                    behavior.id,
                    *handle,
                    *xplane.command
                });
                continue;
            }

            if (behavior.valueType ==
                    cockpitlink::catalog::ValueType::Int &&
                xplane.read && xplane.read->scale)
            {
                const auto& operation = *xplane.read;
                const auto& scale = *operation.scale;
                intReadBindings.push_back({
                    behavior.id,
                    *handle,
                    operation.dataRef,
                    operation.index.value_or(
                        operation.indices.empty() ? 0 :
                            operation.indices.front()),
                    operation.type == "float_array",
                    scale.fromMin,
                    scale.fromMax,
                    scale.toMin,
                    scale.toMax
                });
            }

            if (behavior.valueType ==
                    cockpitlink::catalog::ValueType::Boolean &&
                xplane.read)
            {
                BoolBinding binding;
                binding.behaviorId = behavior.id;
                binding.handle = *handle;
                binding.readDataRef = xplane.read->dataRef;
                binding.writeStrategy =
                    xplane.writeStrategy.value_or(
                        cockpitlink::catalog::WriteStrategy::Unsupported);
                binding.toggleCommand =
                    xplane.toggleCommand.value_or("");

                if (xplane.write)
                {
                    binding.writeDataRef = xplane.write->dataRef;
                }

                boolBindings.push_back(std::move(binding));
                continue;
            }

            if (behavior.valueType !=
                    cockpitlink::catalog::ValueType::Int ||
                !xplane.write ||
                !xplane.write->scale)
            {
                continue;
            }

            const auto& operation = *xplane.write;
            const auto& scale = *operation.scale;
            PercentAxisBinding binding;
            binding.behaviorId = behavior.id;
            binding.handle = *handle;
            binding.dataRef = operation.dataRef;
            binding.xplaneElement =
                operation.index.value_or(
                    operation.indices.empty() ?
                        0 :
                        operation.indices.front());
            binding.xplaneElementCount =
                operation.indices.empty() ?
                    1 :
                    static_cast<int>(operation.indices.size());
            binding.inputMinimum =
                static_cast<int>(scale.fromMin);
            binding.inputMaximum =
                static_cast<int>(scale.fromMax);
            binding.outputMinimum =
                static_cast<float>(scale.toMin);
            binding.outputMaximum =
                static_cast<float>(scale.toMax);
            binding.xplaneArray =
                operation.type == "float_array";
            percentAxisBindings.push_back(std::move(binding));
        }

        std::ostringstream message;
        message
            << "CockpitLink: loaded "
            << catalog->name()
            << " from "
            << loadedPath.string()
            << " ("
            << boolBindings.size()
            << " bool, "
            << percentAxisBindings.size()
            << " axis, "
            << commandBindings.size()
            << " command, "
            << intReadBindings.size()
            << " integer read bindings).\n";
        debugLog(message.str());
        return true;
    }

    bool shouldProbe(
        cockpitlink::serial::SerialDeviceKind kind)
    {
        using cockpitlink::serial::SerialDeviceKind;

        return kind == SerialDeviceKind::ArduinoCompatible ||
            kind == SerialDeviceKind::UsbSerial ||
            kind == SerialDeviceKind::Unknown;
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

    std::string formatFloat(
        float value)
    {
        std::ostringstream output;
        output
            << std::fixed
            << std::setprecision(3)
            << value;
        return output.str();
    }

    const PercentAxisBinding* percentAxisByBehaviorId(
        std::string_view behaviorId)
    {
        const auto found =
            std::find_if(
                percentAxisBindings.begin(),
                percentAxisBindings.end(),
                [behaviorId](const PercentAxisBinding& binding)
                {
                    return binding.behaviorId == behaviorId;
                });

        return found == percentAxisBindings.end() ?
            nullptr :
            &*found;
    }

    const PercentAxisBinding* percentAxisByHandle(
        std::uint16_t handle)
    {
        const auto found =
            std::find_if(
                percentAxisBindings.begin(),
                percentAxisBindings.end(),
                [handle](const PercentAxisBinding& binding)
                {
                    return binding.handle == handle;
                });

        return found == percentAxisBindings.end() ?
            nullptr :
            &*found;
    }

    const BoolBinding* boolBindingByBehaviorId(
        std::string_view behaviorId)
    {
        const auto found =
            std::find_if(
                boolBindings.begin(),
                boolBindings.end(),
                [behaviorId](const BoolBinding& binding)
                {
                    return binding.behaviorId == behaviorId;
                });

        return found == boolBindings.end() ?
            nullptr :
            &*found;
    }

    const BoolBinding* boolBindingByHandle(
        std::uint16_t handle)
    {
        const auto found =
            std::find_if(
                boolBindings.begin(),
                boolBindings.end(),
                [handle](const BoolBinding& binding)
                {
                    return binding.handle == handle;
                });

        return found == boolBindings.end() ?
            nullptr :
            &*found;
    }

    const CommandBinding* commandBindingByBehaviorId(
        std::string_view behaviorId)
    {
        const auto found = std::find_if(
            commandBindings.begin(),
            commandBindings.end(),
            [behaviorId](const CommandBinding& binding)
            {
                return binding.behaviorId == behaviorId;
            });
        return found == commandBindings.end() ? nullptr : &*found;
    }

    const CommandBinding* commandBindingByHandle(
        std::uint16_t handle)
    {
        const auto found = std::find_if(
            commandBindings.begin(),
            commandBindings.end(),
            [handle](const CommandBinding& binding)
            {
                return binding.handle == handle;
            });
        return found == commandBindings.end() ? nullptr : &*found;
    }

    const IntReadBinding* intReadBindingByBehaviorId(
        std::string_view behaviorId)
    {
        const auto found = std::find_if(
            intReadBindings.begin(), intReadBindings.end(),
            [behaviorId](const IntReadBinding& binding)
            { return binding.behaviorId == behaviorId; });
        return found == intReadBindings.end() ? nullptr : &*found;
    }

    const IntReadBinding* intReadBindingByHandle(std::uint16_t handle)
    {
        const auto found = std::find_if(
            intReadBindings.begin(), intReadBindings.end(),
            [handle](const IntReadBinding& binding)
            { return binding.handle == handle; });
        return found == intReadBindings.end() ? nullptr : &*found;
    }

    void copyPluginString(
        char* target,
        const char* value)
    {
        std::strncpy(target, value, 255);
        target[255] = '\0';
    }

    class CockpitLinkXPlaneRuntime
    {
    public:
        CockpitLinkXPlaneRuntime()
        {
            createMenu();
            reconnect();
        }

        ~CockpitLinkXPlaneRuntime()
        {
            disconnect();
            destroyMenu();
        }

        float flightLoop()
        {
            tick();
            drainNextXPlaneCommand();
            return -1.0f;
        }

        void handleMenuAction(
            MenuAction action)
        {
            switch (action)
            {
            case MenuAction::Reconnect:
                reconnect();
                break;

            case MenuAction::Disconnect:
                disconnect();
                break;
            }
        }

    private:
        struct PendingBehaviorAssignment
        {
            std::uint8_t requestId = 0;
            std::uint16_t handle = 0;
            cockpitlink::protocol::ValueType valueType =
                cockpitlink::protocol::ValueType::Boolean;
        };

        struct PendingXPlaneCommand
        {
            std::string behaviorId;
            std::string command;
            cockpitlink::protocol::CommandActionKind action =
                cockpitlink::protocol::CommandActionKind::Trigger;
        };

        static void menuHandler(
            void* menuRef,
            void* itemRef)
        {
            auto* runtime =
                static_cast<CockpitLinkXPlaneRuntime*>(menuRef);

            if (runtime == nullptr)
            {
                return;
            }

            runtime->handleMenuAction(
                static_cast<MenuAction>(
                    reinterpret_cast<std::intptr_t>(itemRef)));
        }

        static void* menuItemRef(
            MenuAction action)
        {
            return reinterpret_cast<void*>(
                static_cast<std::intptr_t>(action));
        }

        void createMenu()
        {
            XPLMMenuID pluginsMenu =
                XPLMFindPluginsMenu();

            if (pluginsMenu == nullptr)
            {
                debugLog(
                    "CockpitLink: unable to find plugins menu.\n");
                return;
            }

            pluginsMenuItemIndex_ =
                XPLMAppendMenuItem(
                    pluginsMenu,
                    "CockpitLink",
                    nullptr,
                    0);

            menu_ =
                XPLMCreateMenu(
                    "CockpitLink",
                    pluginsMenu,
                    pluginsMenuItemIndex_,
                    menuHandler,
                    this);

            if (menu_ == nullptr)
            {
                debugLog(
                    "CockpitLink: unable to create menu.\n");
                return;
            }

            XPLMAppendMenuItem(
                menu_,
                "Reconnect Devices",
                menuItemRef(MenuAction::Reconnect),
                0);
            XPLMAppendMenuItem(
                menu_,
                "Disconnect Devices",
                menuItemRef(MenuAction::Disconnect),
                0);
        }

        void destroyMenu()
        {
            if (menu_ != nullptr)
            {
                XPLMDestroyMenu(menu_);
                menu_ = nullptr;
            }

            if (pluginsMenuItemIndex_ >= 0)
            {
                if (XPLMMenuID pluginsMenu = XPLMFindPluginsMenu())
                {
                    XPLMRemoveMenuItem(
                        pluginsMenu,
                        pluginsMenuItemIndex_);
                }

                pluginsMenuItemIndex_ = -1;
            }
        }

        void reconnect()
        {
            disconnect();

            ports_.clear();
            portIndex_ = 0;
            state_ = ConnectionState::Idle;
            nextActionAt_ =
                std::chrono::steady_clock::now();

            const auto detectedPorts =
                enumerator_.enumerate();

            for (const auto& port : detectedPorts)
            {
                if (shouldProbe(port.kind))
                {
                    ports_.push_back(port.portName);
                }
            }

            std::ostringstream message;
            message
                << "CockpitLink: reconnect requested, "
                << ports_.size()
                << " candidate port(s).\n";
            debugLog(message.str());
        }

        void disconnect()
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
            percentAxisAssignments_ = 0;
            beaconAssignments_ = 0;
            boolSubscribed_.fill(false);
            intSubscribed_.fill(false);
            hasSentBoolValue_.fill(false);
            boolNextDue_.fill({});
            lastAxisPercent_.fill(-1);
            pendingAssignments_.clear();
            pendingXPlaneCommands_.clear();
            nextAssignmentAt_ = {};
        }

        void tick()
        {
            const auto now =
                std::chrono::steady_clock::now();

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
                readConnectedPort();
                tickSubscriptions(now);
                break;
            }

            sendNextBehaviorAssignment(now);
        }

        void startNextPort(
            std::chrono::steady_clock::time_point now)
        {
            if (transport_ || portIndex_ >= ports_.size())
            {
                return;
            }

            const std::string portName =
                ports_[portIndex_++];

            transport_ =
                std::make_unique<cockpitlink::serial::WindowsSerialTransport>(
                    portName,
                    baudRate,
                    cockpitlink::serial::WindowsSerialControlMode::DtrRtsDisabled);

            if (!transport_->open())
            {
                std::ostringstream message;
                message
                    << "CockpitLink: unable to open "
                    << portName
                    << ".\n";
                debugLog(message.str());
                transport_.reset();
                return;
            }

            session_ =
                std::make_unique<
                    cockpitlink::transport::TransportSession>(
                    *transport_,
                    cockpitlink::transport::TransportSessionOptions{
                        .readBufferSize = 256,
                        .maximumReadPasses = 4,
                        .maximumMessagesPerTick = 16,
                        .maximumWriteBytesPerTick = 64
                    });
            connectedPort_ = portName;
            state_ = ConnectionState::WaitingForSettle;
            nextActionAt_ = now + openSettleDelay;
            portDeadline_ = now + portTimeout;

            std::ostringstream message;
            message
                << "CockpitLink: opened "
                << connectedPort_
                << ", waiting for board settle.\n";
            debugLog(message.str());
        }

        void tickProbe(
            std::chrono::steady_clock::time_point now)
        {
            if (!transport_)
            {
                state_ = ConnectionState::Idle;
                return;
            }

            if (now >= nextActionAt_)
            {
                sendHello();
                nextActionAt_ = now + probeRetryInterval;
            }

            readConnectedPort();

            if (helloAckReceived_)
            {
                state_ = ConnectionState::Connected;
                debugLog(
                    "CockpitLink: device connected.\n");
                return;
            }

            if (now >= portDeadline_)
            {
                std::ostringstream message;
                message
                    << "CockpitLink: no response from "
                    << connectedPort_
                    << ".\n";
                debugLog(message.str());
                disconnect();
                state_ = ConnectionState::Idle;
            }
        }

        void readConnectedPort()
        {
            if (!transport_ ||
                !transport_->isOpen() ||
                !session_)
            {
                return;
            }

            const auto tick =
                session_->tick();

            for (const auto& frame : tick.frames)
            {
                handleFrame(frame);
            }
        }

        void handleFrame(
            const cockpitlink::protocol::Frame& frame)
        {
            using cockpitlink::protocol::MessageType;

            switch (frame.type)
            {
            case MessageType::HelloAck:
                handleHelloAck(frame);
                break;

            case MessageType::BehaviorRequest:
                handleBehaviorRequest(frame);
                break;

            case MessageType::Subscribe:
                handleSubscribe(frame);
                break;

            case MessageType::ValueUpdate:
                handleValueUpdate(frame);
                break;

            case MessageType::CommandAction:
                handleCommandAction(frame);
                break;

            default:
                break;
            }
        }

        void handleSubscribe(
            const cockpitlink::protocol::Frame& frame)
        {
            const auto subscribe =
                cockpitlink::protocol::decodeSubscribePayload(frame.payload);

            if (!subscribe)
            {
                return;
            }

            if (subscribe->valueType ==
                cockpitlink::protocol::ValueType::Int)
            {
                const auto* binding =
                    intReadBindingByHandle(subscribe->handle);
                if (binding == nullptr)
                {
                    return;
                }
                intSubscribed_[binding->handle] = true;
                intRates_[binding->handle] = std::chrono::milliseconds{
                    subscribe->rateMs == 0 ? 250 : subscribe->rateMs };
                intNextDue_[binding->handle] =
                    std::chrono::steady_clock::now();
                hasSentIntValue_[binding->handle] = false;
                return;
            }

            const auto* binding =
                boolBindingByHandle(subscribe->handle);
            if (binding == nullptr || subscribe->valueType !=
                cockpitlink::protocol::ValueType::Boolean)
            {
                return;
            }

            boolSubscribed_[binding->handle] = true;
            boolRates_[binding->handle] =
                std::chrono::milliseconds{
                    subscribe->rateMs == 0 ?
                        100 :
                        subscribe->rateMs
                };
            hasSentBoolValue_[binding->handle] = false;
            boolNextDue_[binding->handle] =
                std::chrono::steady_clock::now();

            sendBoolValue(
                *binding,
                true);
        }

        void handleHelloAck(
            const cockpitlink::protocol::Frame& frame)
        {
            const auto hello =
                cockpitlink::protocol::decodeHelloPayload(frame.payload);

            if (!hello)
            {
                return;
            }

            helloAckReceived_ = true;

            std::ostringstream message;
            message
                << "CockpitLink: "
                << connectedPort_
                << " identified as "
                << hello->deviceName
                << " firmware "
                << hello->firmwareVersion
                << ".\n";
            debugLog(message.str());
        }

        void handleBehaviorRequest(
            const cockpitlink::protocol::Frame& frame)
        {
            const auto request =
                cockpitlink::protocol::decodeBehaviorRequestPayload(
                    frame.payload);

            if (!request)
            {
                return;
            }

            if (const auto* axis =
                percentAxisByBehaviorId(request->behaviorId))
            {
                queueBehaviorAssignment(
                    request->requestId,
                    axis->handle,
                    cockpitlink::protocol::ValueType::Int);
                ++percentAxisAssignments_;

                std::ostringstream message;
                message
                    << "CockpitLink: assigned "
                    << axis->behaviorId
                    << ".\n";
                debugLog(message.str());
            }
            else if (const auto* command =
                commandBindingByBehaviorId(request->behaviorId))
            {
                queueBehaviorAssignment(
                    request->requestId,
                    command->handle,
                    cockpitlink::protocol::ValueType::Boolean);
            }
            else if (const auto* value =
                intReadBindingByBehaviorId(request->behaviorId))
            {
                queueBehaviorAssignment(
                    request->requestId,
                    value->handle,
                    cockpitlink::protocol::ValueType::Int);
            }
            else if (const auto* binding =
                boolBindingByBehaviorId(request->behaviorId))
            {
                queueBehaviorAssignment(
                    request->requestId,
                    binding->handle,
                    cockpitlink::protocol::ValueType::Boolean);
                ++beaconAssignments_;
            }
            else
            {
                std::ostringstream message;
                message
                    << "CockpitLink: unsupported behavior request "
                    << request->behaviorId
                    << ".\n";
                debugLog(message.str());
            }
        }

        void handleCommandAction(
            const cockpitlink::protocol::Frame& frame)
        {
            const auto action =
                cockpitlink::protocol::decodeCommandActionPayload(
                    frame.payload);
            const auto* binding =
                action ? commandBindingByHandle(action->handle) : nullptr;

            if (!action || binding == nullptr)
            {
                return;
            }

            pendingXPlaneCommands_.push_back({
                binding->behaviorId,
                binding->command,
                action->action
            });
        }

        void drainNextXPlaneCommand()
        {
            if (pendingXPlaneCommands_.empty())
            {
                return;
            }

            PendingXPlaneCommand pending =
                std::move(pendingXPlaneCommands_.front());
            pendingXPlaneCommands_.pop_front();

            const auto command =
                XPLMFindCommand(pending.command.c_str());

            if (command == nullptr)
            {
                debugLog(
                    "CockpitLink: command not found for " +
                    pending.behaviorId + ": " +
                    pending.command + ".\n");
                return;
            }

            using cockpitlink::protocol::CommandActionKind;
            switch (pending.action)
            {
            case CommandActionKind::Trigger:
                XPLMCommandOnce(command);
                break;
            case CommandActionKind::Begin:
                XPLMCommandBegin(command);
                break;
            case CommandActionKind::End:
                XPLMCommandEnd(command);
                break;
            }
        }

        void tickSubscriptions(
            std::chrono::steady_clock::time_point now)
        {
            for (const auto& binding : boolBindings)
            {
                if (!boolSubscribed_[binding.handle] ||
                    now < boolNextDue_[binding.handle])
                {
                    continue;
                }

                sendBoolValue(
                    binding,
                    false);
            }

            for (const auto& binding : intReadBindings)
            {
                if (intSubscribed_[binding.handle] &&
                    now >= intNextDue_[binding.handle])
                {
                    sendIntValue(binding);
                }
            }
        }

        void sendIntValue(const IntReadBinding& binding)
        {
            auto& dataRef = intDataRefs_[binding.handle];
            if (dataRef == nullptr)
            {
                dataRef = XPLMFindDataRef(binding.dataRef.c_str());
                if (dataRef == nullptr)
                {
                    debugLog("CockpitLink: integer read dataref not found for " +
                        binding.behaviorId + ".\n");
                    return;
                }
            }

            float raw = 0.0f;
            if (binding.xplaneArray)
            {
                XPLMGetDatavf(dataRef, &raw, binding.element, 1);
            }
            else
            {
                raw = XPLMGetDataf(dataRef);
            }

            const double normalized =
                (raw - binding.inputMinimum) /
                (binding.inputMaximum - binding.inputMinimum);
            const auto value = static_cast<std::int32_t>(std::lround(
                binding.outputMinimum +
                normalized * (binding.outputMaximum - binding.outputMinimum)));
            intNextDue_[binding.handle] = std::chrono::steady_clock::now() +
                intRates_[binding.handle];

            if (hasSentIntValue_[binding.handle] &&
                lastIntValue_[binding.handle] == value)
            {
                return;
            }

            queueLatestFrame(binding.handle, {
                cockpitlink::protocol::MessageType::ValueUpdate,
                0,
                nextSequence_++,
                cockpitlink::protocol::encodeIntValueUpdatePayload(
                    binding.handle, value)
            });
            lastIntValue_[binding.handle] = value;
            hasSentIntValue_[binding.handle] = true;
        }

        void sendBoolValue(
            const BoolBinding& binding,
            bool force)
        {
            if (!transport_ ||
                !transport_->isOpen())
            {
                return;
            }

            auto& dataRef =
                boolDataRefs_[binding.handle];

            if (dataRef == nullptr)
            {
                const std::string dataRefName{
                    binding.readDataRef
                };
                dataRef =
                    XPLMFindDataRef(dataRefName.c_str());

                if (dataRef == nullptr)
                {
                    std::ostringstream message;
                    message
                        << "CockpitLink: bool dataref not found for "
                        << binding.behaviorId
                        << ": "
                        << binding.readDataRef
                        << ".\n";
                    debugLog(message.str());
                    return;
                }
            }

            const bool value =
                XPLMGetDatai(dataRef) != 0;

            boolNextDue_[binding.handle] =
                std::chrono::steady_clock::now() +
                boolRates_[binding.handle];

            if (!force &&
                hasSentBoolValue_[binding.handle] &&
                value == lastBoolValue_[binding.handle])
            {
                return;
            }

            queueLatestFrame(
                binding.handle,
                {
                    cockpitlink::protocol::MessageType::ValueUpdate,
                    0,
                    nextSequence_++,
                    cockpitlink::protocol::encodeBoolValueUpdatePayload(
                        binding.handle,
                        value)
                });

            lastBoolValue_[binding.handle] = value;
            hasSentBoolValue_[binding.handle] = true;

            std::ostringstream message;
            message
                << "CockpitLink: sent "
                << binding.behaviorId
                << " "
                << (value ? "true" : "false")
                << ".\n";
            debugLog(message.str());
        }

        void handleValueUpdate(
            const cockpitlink::protocol::Frame& frame)
        {
            const auto update =
                cockpitlink::protocol::decodeValueUpdatePayload(
                    frame.payload);

            if (!update)
            {
                return;
            }

            const auto* boolBinding =
                boolBindingByHandle(update->handle);

            if (boolBinding != nullptr &&
                update->valueType ==
                    cockpitlink::protocol::ValueType::Boolean &&
                update->value.size() == 1)
            {
                writeBoolValue(
                    *boolBinding,
                    std::to_integer<unsigned char>(
                        update->value[0]) != 0);
                return;
            }

            if (update->valueType != cockpitlink::protocol::ValueType::Int ||
                update->value.size() != 4)
            {
                return;
            }

            const auto receivedValue =
                readI32(update->value);

            {
                std::ostringstream message;
                message
                    << "CockpitLink: raw int update handle "
                    << update->handle
                    << " value "
                    << receivedValue
                    << ".\n";
                debugLog(message.str());
            }

            const auto* axis =
                percentAxisByHandle(update->handle);

            if (axis == nullptr)
            {
                return;
            }

            const auto canonicalValue =
                std::clamp<std::int32_t>(
                    receivedValue,
                    axis->inputMinimum,
                    axis->inputMaximum);
            const float normalized =
                static_cast<float>(
                    canonicalValue - axis->inputMinimum) /
                static_cast<float>(
                    axis->inputMaximum - axis->inputMinimum);
            const float xplaneValue =
                axis->outputMinimum +
                ((axis->outputMaximum - axis->outputMinimum) *
                    normalized);

            if (canonicalValue !=
                lastAxisPercent_[axis->handle])
            {
                lastAxisPercent_[axis->handle] =
                    canonicalValue;

                std::ostringstream message;
                message
                    << "CockpitLink: "
                    << axis->behaviorId
                    << " "
                    << canonicalValue
                    << " -> X-Plane value "
                    << formatFloat(xplaneValue)
                    << " ";

                if (axis->xplaneArray)
                {
                    message
                        << "element "
                        << axis->xplaneElement;

                    if (axis->xplaneElementCount > 1)
                    {
                        message
                            << ".."
                            << (axis->xplaneElement +
                                axis->xplaneElementCount - 1);
                    }
                }
                else
                {
                    message
                        << "scalar";
                }

                message
                    << ".\n";
                debugLog(message.str());
            }

            writePercentAxisRatio(
                xplaneValue,
                *axis);
        }

        void writeBoolValue(
            const BoolBinding& binding,
            bool value)
        {
            auto& dataRef =
                boolDataRefs_[binding.handle];

            if (dataRef == nullptr)
            {
                const std::string dataRefName{
                    !binding.writeDataRef.empty() ?
                        binding.writeDataRef :
                        binding.readDataRef
                };
                dataRef =
                    XPLMFindDataRef(dataRefName.c_str());

                if (dataRef == nullptr)
                {
                    std::ostringstream message;
                    message
                        << "CockpitLink: bool dataref not found for "
                        << binding.behaviorId
                        << ": "
                        << dataRefName
                        << ".\n";
                    debugLog(message.str());
                    return;
                }
            }

            using cockpitlink::catalog::WriteStrategy;

            if (binding.writeStrategy == WriteStrategy::DirectSet)
            {
                XPLMSetDatai(
                    dataRef,
                    value ? 1 : 0);
            }
            else if (binding.writeStrategy ==
                WriteStrategy::SetViaToggleWhenKnown)
            {
                const bool currentValue =
                    XPLMGetDatai(dataRef) != 0;

                if (currentValue != value)
                {
                    pendingXPlaneCommands_.push_back({
                        binding.behaviorId,
                        binding.toggleCommand,
                        cockpitlink::protocol::CommandActionKind::Trigger
                    });
                }
            }
            else
            {
                debugLog(
                    "CockpitLink: behavior is not writable: " +
                    binding.behaviorId + ".\n");
                return;
            }

            lastBoolValue_[binding.handle] = value;
            hasSentBoolValue_[binding.handle] = true;

            std::ostringstream message;
            message
                << "CockpitLink: received "
                << binding.behaviorId
                << " "
                << (value ? "true" : "false")
                << " from device.\n";
            debugLog(message.str());
        }

        void writePercentAxisRatio(
            float xplaneValue,
            const PercentAxisBinding& axis)
        {
            auto& dataRef =
                axisDataRefs_[axis.handle];

            if (dataRef == nullptr)
            {
                const std::string dataRefName{
                    axis.dataRef
                };
                dataRef =
                    XPLMFindDataRef(dataRefName.c_str());

                if (dataRef == nullptr)
                {
                    std::ostringstream message;
                    message
                        << "CockpitLink: dataref not found for "
                        << axis.behaviorId
                        << ": "
                        << axis.dataRef
                        << ".\n";
                    debugLog(message.str());
                    return;
                }
            }

            if (!axis.xplaneArray)
            {
                XPLMSetDataf(
                    dataRef,
                    xplaneValue);
                return;
            }

            std::array<float, 16> values{};

            for (int index = 0;
                index < axis.xplaneElementCount &&
                    index < static_cast<int>(values.size());
                ++index)
            {
                values[static_cast<std::size_t>(index)] =
                    xplaneValue;
            }

            XPLMSetDatavf(
                dataRef,
                values.data(),
                axis.xplaneElement,
                axis.xplaneElementCount);
        }

        void sendHello()
        {
            sendFrame({
                cockpitlink::protocol::MessageType::Hello,
                0,
                nextSequence_++,
                cockpitlink::protocol::encodeHelloPayload({
                    "CockpitLinkXPlane",
                    "xplane",
                    static_cast<std::uint16_t>(
                        cockpitlink::protocol::maximumPayloadSize),
                    cockpitlink::protocol::minimumProtocolVersion,
                    cockpitlink::protocol::protocolVersion,
                    cockpitlink::protocol::CapabilitySerial |
                        cockpitlink::protocol::CapabilityBehaviorIds |
                        cockpitlink::protocol::CapabilityBinaryValues |
                        cockpitlink::protocol::CapabilityDecodedDiagnostics
                })
            });
        }

        void queueBehaviorAssignment(
            std::uint8_t requestId,
            std::uint16_t handle,
            cockpitlink::protocol::ValueType valueType)
        {
            pendingAssignments_.push_back({
                requestId,
                handle,
                valueType
            });
        }

        void sendNextBehaviorAssignment(
            std::chrono::steady_clock::time_point now)
        {
            if (pendingAssignments_.empty() ||
                !transport_ ||
                !transport_->isOpen() ||
                now < nextAssignmentAt_)
            {
                return;
            }

            const auto assignment =
                pendingAssignments_.front();
            pendingAssignments_.pop_front();
            nextAssignmentAt_ =
                now + assignmentInterval;

            sendFrame({
                cockpitlink::protocol::MessageType::BehaviorAssignment,
                0,
                nextSequence_++,
                cockpitlink::protocol::encodeBehaviorAssignmentPayload({
                    assignment.requestId,
                    assignment.handle,
                    assignment.valueType,
                    cockpitlink::protocol::CapabilityBehaviorIds |
                        cockpitlink::protocol::CapabilityBinaryValues
                })
            });
        }

        void sendFrame(
            const cockpitlink::protocol::Frame& frame)
        {
            if (!session_)
            {
                return;
            }

            session_->queueReliable(frame);
        }

        void queueLatestFrame(
            std::uint16_t key,
            const cockpitlink::protocol::Frame& frame)
        {
            if (!session_)
            {
                return;
            }

            session_->queueLatest(key, frame);
        }

        cockpitlink::serial::WindowsSerialEnumerator enumerator_;
        std::vector<std::string> ports_;
        std::size_t portIndex_ = 0;
        std::unique_ptr<cockpitlink::serial::WindowsSerialTransport>
            transport_;
        std::unique_ptr<cockpitlink::transport::TransportSession>
            session_;
        ConnectionState state_ = ConnectionState::Idle;
        std::chrono::steady_clock::time_point nextActionAt_{};
        std::chrono::steady_clock::time_point portDeadline_{};
        std::string connectedPort_;
        std::uint16_t nextSequence_ = 1;
        bool helloAckReceived_ = false;
        int percentAxisAssignments_ = 0;
        int beaconAssignments_ = 0;
        static constexpr std::size_t maxBehaviorHandle_ =
            maxBehaviorHandles;
        std::array<bool, maxBehaviorHandle_> boolSubscribed_{};
        std::array<bool, maxBehaviorHandle_> lastBoolValue_{};
        std::array<bool, maxBehaviorHandle_> hasSentBoolValue_{};
        std::array<std::chrono::milliseconds, maxBehaviorHandle_>
            boolRates_{};
        std::array<std::chrono::steady_clock::time_point, maxBehaviorHandle_>
            boolNextDue_{};
        std::array<XPLMDataRef, maxBehaviorHandle_> boolDataRefs_{};
        std::array<bool, maxBehaviorHandle_> intSubscribed_{};
        std::array<bool, maxBehaviorHandle_> hasSentIntValue_{};
        std::array<std::int32_t, maxBehaviorHandle_> lastIntValue_{};
        std::array<std::chrono::milliseconds, maxBehaviorHandle_> intRates_{};
        std::array<std::chrono::steady_clock::time_point, maxBehaviorHandle_>
            intNextDue_{};
        std::array<XPLMDataRef, maxBehaviorHandle_> intDataRefs_{};
        std::array<std::int32_t, maxBehaviorHandle_> lastAxisPercent_{
            -1,
            -1,
            -1,
            -1,
            -1,
            -1,
            -1,
            -1
        };
        std::array<XPLMDataRef, maxBehaviorHandle_> axisDataRefs_{};
        std::deque<PendingBehaviorAssignment> pendingAssignments_;
        std::deque<PendingXPlaneCommand> pendingXPlaneCommands_;
        std::chrono::steady_clock::time_point nextAssignmentAt_{};
        XPLMMenuID menu_ = nullptr;
        int pluginsMenuItemIndex_ = -1;
    };

    std::unique_ptr<CockpitLinkXPlaneRuntime> runtime;

    float flightLoopCallback(
        float,
        float,
        int,
        void*)
    {
        if (runtime)
        {
            return runtime->flightLoop();
        }

        return 0.0f;
    }
}

extern "C"
{
    PLUGIN_API int XPluginStart(
        char* outName,
        char* outSig,
        char* outDesc)
    {
        copyPluginString(outName, "CockpitLink");
        copyPluginString(outSig, "com.cockpitlink.xplane");
        copyPluginString(outDesc, "CockpitLink X-Plane behavior bridge.");

        if (!loadCatalogBindings())
        {
            return 0;
        }

        runtime =
            std::make_unique<CockpitLinkXPlaneRuntime>();

        XPLMRegisterFlightLoopCallback(
            flightLoopCallback,
            -1.0f,
            nullptr);

        debugLog(
            "CockpitLink: plugin started.\n");

        return 1;
    }

    PLUGIN_API void XPluginStop()
    {
        XPLMUnregisterFlightLoopCallback(
            flightLoopCallback,
            nullptr);

        runtime.reset();

        debugLog(
            "CockpitLink: plugin stopped.\n");
    }

    PLUGIN_API int XPluginEnable()
    {
        debugLog(
            "CockpitLink: plugin enabled.\n");
        return 1;
    }

    PLUGIN_API void XPluginDisable()
    {
        debugLog(
            "CockpitLink: plugin disabled.\n");
    }

    PLUGIN_API void XPluginReceiveMessage(
        XPLMPluginID,
        int,
        void*)
    {
    }
}
