#include <cockpitlink/protocol/Frame.h>
#include <cockpitlink/protocol/FrameParser.h>
#include <cockpitlink/protocol/Payloads.h>
#include <cockpitlink/serial/SerialDeviceKind.h>
#include <cockpitlink/serial/WindowsSerialEnumerator.h>
#include <cockpitlink/serial/WindowsSerialTransport.h>

#include <XPLMDataAccess.h>
#include <XPLMMenus.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
        std::string_view behaviorId;
        std::uint16_t handle = 0;
        std::string_view dataRef;
    };

    constexpr std::array<BoolBinding, 3> boolBindings{ {
        {
            "lights.beacon",
            0,
            "sim/cockpit/electrical/beacon_lights_on"
        },
        {
            "lights.nav",
            7,
            "sim/cockpit/electrical/nav_lights_on"
        },
        {
            "lights.strobe",
            8,
            "sim/cockpit/electrical/strobe_lights_on"
        }
    } };

    struct PercentAxisBinding
    {
        std::string_view behaviorId;
        std::uint16_t handle = 0;
        std::string_view dataRef;
        int xplaneElement = 0;
        int xplaneElementCount = 1;
        int inputMinimum = 0;
        int inputMaximum = 100;
        float outputMinimum = 0.0f;
        float outputMaximum = 1.0f;
        bool xplaneArray = true;
    };

    constexpr std::array<PercentAxisBinding, 9> percentAxisBindings{ {
        {
            "engine.all.throttle",
            9,
            "sim/cockpit2/engine/actuators/throttle_ratio",
            0,
            2,
            0,
            100,
            0.0f,
            1.0f
        },
        {
            "engine.1.throttle",
            1,
            "sim/cockpit2/engine/actuators/throttle_ratio",
            0,
            1,
            0,
            100,
            0.0f,
            1.0f
        },
        {
            "engine.2.throttle",
            2,
            "sim/cockpit2/engine/actuators/throttle_ratio",
            1,
            1,
            0,
            100,
            0.0f,
            1.0f
        },
        {
            "engine.1.prop_rpm",
            3,
            "sim/cockpit2/engine/actuators/prop_rotation_speed_rad_sec",
            0,
            1,
            0,
            100,
            73.29f,
            282.70f
        },
        {
            "engine.2.prop_rpm",
            4,
            "sim/cockpit2/engine/actuators/prop_rotation_speed_rad_sec",
            1,
            1,
            0,
            100,
            73.29f,
            282.70f
        },
        {
            "engine.1.mixture",
            5,
            "sim/cockpit2/engine/actuators/mixture_ratio",
            0,
            1,
            0,
            100,
            0.0f,
            1.0f
        },
        {
            "engine.2.mixture",
            6,
            "sim/cockpit2/engine/actuators/mixture_ratio",
            1,
            1,
            0,
            100,
            0.0f,
            1.0f
        },
        {
            "flight.roll",
            10,
            "sim/cockpit2/controls/yoke_roll_ratio",
            0,
            1,
            -100,
            100,
            -1.0f,
            1.0f,
            false
        },
        {
            "flight.pitch",
            11,
            "sim/cockpit2/controls/yoke_pitch_ratio",
            0,
            1,
            -100,
            100,
            -1.0f,
            1.0f,
            false
        }
    } };
    constexpr auto baudRate = 115200u;
    constexpr auto openSettleDelay =
        std::chrono::seconds{ 3 };
    constexpr auto probeRetryInterval =
        std::chrono::milliseconds{ 250 };
    constexpr auto portTimeout =
        std::chrono::seconds{ 6 };

    void debugLog(
        std::string_view message)
    {
        const std::string stableMessage{ message };
        XPLMDebugString(stableMessage.c_str());
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

            transport_.reset();
            parser_.reset();
            connectedPort_.clear();
            state_ = ConnectionState::Idle;
            helloAckReceived_ = false;
            percentAxisAssignments_ = 0;
            beaconAssignments_ = 0;
            boolSubscribed_.fill(false);
            hasSentBoolValue_.fill(false);
            boolNextDue_.fill({});
            lastAxisPercent_.fill(-1);
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
            if (!transport_ || !transport_->isOpen())
            {
                return;
            }

            std::array<std::byte, 512> buffer{};
            constexpr int maximumReadPasses = 16;

            for (int pass = 0;
                pass < maximumReadPasses;
                ++pass)
            {
                const std::size_t bytesRead =
                    transport_->read(buffer);

                if (bytesRead == 0)
                {
                    return;
                }

                const auto frames =
                    parser_.push(
                        std::span<const std::byte>{
                            buffer.data(),
                            bytesRead
                        });

                for (const auto& frame : frames)
                {
                    handleFrame(frame);
                }
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

            default:
                break;
            }
        }

        void handleSubscribe(
            const cockpitlink::protocol::Frame& frame)
        {
            const auto subscribe =
                cockpitlink::protocol::decodeSubscribePayload(frame.payload);

            const auto* binding =
                subscribe ?
                    boolBindingByHandle(subscribe->handle) :
                    nullptr;

            if (!subscribe ||
                binding == nullptr ||
                subscribe->valueType !=
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
                sendBehaviorAssignment(
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
            else if (const auto* binding =
                boolBindingByBehaviorId(request->behaviorId))
            {
                sendBehaviorAssignment(
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
                    binding.dataRef
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
                        << binding.dataRef
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

            sendFrame({
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
                    binding.dataRef
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
                        << binding.dataRef
                        << ".\n";
                    debugLog(message.str());
                    return;
                }
            }

            XPLMSetDatai(
                dataRef,
                value ? 1 : 0);

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

        void sendBehaviorAssignment(
            std::uint8_t requestId,
            std::uint16_t handle,
            cockpitlink::protocol::ValueType valueType)
        {
            sendFrame({
                cockpitlink::protocol::MessageType::BehaviorAssignment,
                0,
                nextSequence_++,
                cockpitlink::protocol::encodeBehaviorAssignmentPayload({
                    requestId,
                    handle,
                    valueType,
                    cockpitlink::protocol::CapabilityBehaviorIds |
                        cockpitlink::protocol::CapabilityBinaryValues
                })
            });
        }

        void sendFrame(
            const cockpitlink::protocol::Frame& frame)
        {
            if (!transport_ || !transport_->isOpen())
            {
                return;
            }

            const auto bytes =
                cockpitlink::protocol::encodeFrame(frame);
            transport_->write(bytes);
        }

        cockpitlink::serial::WindowsSerialEnumerator enumerator_;
        std::vector<std::string> ports_;
        std::size_t portIndex_ = 0;
        std::unique_ptr<cockpitlink::serial::WindowsSerialTransport>
            transport_;
        cockpitlink::protocol::FrameParser parser_;
        ConnectionState state_ = ConnectionState::Idle;
        std::chrono::steady_clock::time_point nextActionAt_{};
        std::chrono::steady_clock::time_point portDeadline_{};
        std::string connectedPort_;
        std::uint16_t nextSequence_ = 1;
        bool helloAckReceived_ = false;
        int percentAxisAssignments_ = 0;
        int beaconAssignments_ = 0;
        static constexpr std::size_t maxBehaviorHandle_ = 16;
        std::array<bool, maxBehaviorHandle_> boolSubscribed_{};
        std::array<bool, maxBehaviorHandle_> lastBoolValue_{};
        std::array<bool, maxBehaviorHandle_> hasSentBoolValue_{};
        std::array<std::chrono::milliseconds, maxBehaviorHandle_>
            boolRates_{};
        std::array<std::chrono::steady_clock::time_point, maxBehaviorHandle_>
            boolNextDue_{};
        std::array<XPLMDataRef, maxBehaviorHandle_> boolDataRefs_{};
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
