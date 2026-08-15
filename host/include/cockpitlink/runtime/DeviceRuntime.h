#pragma once

#include <cockpitlink/runtime/ISimulatorAdapter.h>
#include <cockpitlink/serial/WindowsSerialEnumerator.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cockpitlink::serial
{
    class WindowsSerialTransport;
}

namespace cockpitlink::transport
{
    class TransportSession;
}

namespace cockpitlink::runtime
{
    class DeviceRuntime final : public IValuePublisher
    {
    public:
        using Logger = std::function<void(std::string_view)>;

        explicit DeviceRuntime(
            ISimulatorAdapter& adapter,
            std::string hostName,
            Logger logger = {});
        ~DeviceRuntime();

        void reconnect();
        void disconnect();
        void tick();

        bool connected() const;
        const std::string& connectedPort() const;

        void publishBool(std::uint16_t handle, bool value) override;
        void publishInt(std::uint16_t handle, std::int32_t value) override;

    private:
        enum class ConnectionState
        {
            Idle,
            WaitingForSettle,
            Probing,
            Connected
        };

        struct PendingAssignment
        {
            std::uint8_t requestId = 0;
            BehaviorResolution resolution;
        };

        void log(std::string message) const;
        void closeCurrentPort();
        void startNextPort(std::chrono::steady_clock::time_point now);
        void tickProbe(std::chrono::steady_clock::time_point now);
        void readPort();
        void handleFrame(const protocol::Frame& frame);
        void sendHello();
        void sendNextAssignment(std::chrono::steady_clock::time_point now);
        void sendReliable(protocol::Frame frame);
        void sendLatest(std::uint16_t handle, protocol::Frame frame);

        ISimulatorAdapter& adapter_;
        std::string hostName_;
        Logger logger_;
        serial::WindowsSerialEnumerator enumerator_;
        std::vector<std::string> ports_;
        std::size_t portIndex_ = 0;
        std::unique_ptr<serial::WindowsSerialTransport> transport_;
        std::unique_ptr<transport::TransportSession> session_;
        std::string connectedPort_;
        ConnectionState state_ = ConnectionState::Idle;
        bool helloAckReceived_ = false;
        std::chrono::steady_clock::time_point nextActionAt_{};
        std::chrono::steady_clock::time_point portDeadline_{};
        std::chrono::steady_clock::time_point nextAssignmentAt_{};
        std::deque<PendingAssignment> pendingAssignments_;
        std::uint16_t nextSequence_ = 1;
    };
}
