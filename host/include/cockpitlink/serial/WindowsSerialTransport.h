#pragma once

#include <cockpitlink/transport/IByteTransport.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdint>
#include <string>

namespace cockpitlink::serial
{
    enum class WindowsSerialControlMode
    {
        DtrRtsDisabled,
        DtrRtsEnabled
    };

    class WindowsSerialTransport final
        : public transport::IByteTransport
    {
    public:
        explicit WindowsSerialTransport(
            std::string portName,
            std::uint32_t baudRate = 115200,
            WindowsSerialControlMode controlMode =
                WindowsSerialControlMode::DtrRtsDisabled);

        ~WindowsSerialTransport() override;

        WindowsSerialTransport(
            const WindowsSerialTransport&) = delete;

        WindowsSerialTransport& operator=(
            const WindowsSerialTransport&) = delete;

        bool open() override;
        void close() override;
        bool isOpen() const override;

        std::size_t read(
            std::span<std::byte> buffer) override;

        std::size_t write(
            std::span<const std::byte> data) override;

        const std::string& portName() const;

    private:
        bool configurePort();

        std::string portName_;
        std::uint32_t baudRate_;
        WindowsSerialControlMode controlMode_;
        HANDLE handle_ = INVALID_HANDLE_VALUE;
    };
}
