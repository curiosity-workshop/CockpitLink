#include <cockpitlink/serial/WindowsSerialTransport.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace cockpitlink::serial
{
    namespace
    {
        std::string makeWindowsPortPath(
            const std::string& portName)
        {
            if (portName.starts_with(R"(\\.\)"))
            {
                return portName;
            }

            return R"(\\.\)" + portName;
        }
    }

    WindowsSerialTransport::WindowsSerialTransport(
        std::string portName,
        std::uint32_t baudRate,
        WindowsSerialControlMode controlMode)
        : portName_(std::move(portName)),
        baudRate_(baudRate),
        controlMode_(controlMode)
    {
    }

    WindowsSerialTransport::~WindowsSerialTransport()
    {
        close();
    }

    bool WindowsSerialTransport::open()
    {
        if (isOpen())
        {
            return true;
        }

        const std::string devicePath =
            makeWindowsPortPath(portName_);

        handle_ = CreateFileA(
            devicePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (handle_ == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        if (!configurePort())
        {
            close();
            return false;
        }

        PurgeComm(
            handle_,
            PURGE_RXABORT |
            PURGE_RXCLEAR |
            PURGE_TXABORT |
            PURGE_TXCLEAR);

        return true;
    }

    bool WindowsSerialTransport::configurePort()
    {
        DCB configuration{};
        configuration.DCBlength = sizeof(configuration);

        if (!GetCommState(handle_, &configuration))
        {
            return false;
        }

        configuration.BaudRate = baudRate_;
        configuration.ByteSize = 8;
        configuration.StopBits = ONESTOPBIT;
        configuration.Parity = NOPARITY;
        configuration.fBinary = TRUE;
        configuration.fParity = FALSE;
        configuration.fOutxCtsFlow = FALSE;
        configuration.fOutxDsrFlow = FALSE;
        configuration.fDsrSensitivity = FALSE;
        configuration.fOutX = FALSE;
        configuration.fInX = FALSE;

        if (controlMode_ == WindowsSerialControlMode::DtrRtsEnabled)
        {
            configuration.fDtrControl = DTR_CONTROL_ENABLE;
            configuration.fRtsControl = RTS_CONTROL_ENABLE;
        }
        else
        {
            configuration.fDtrControl = DTR_CONTROL_DISABLE;
            configuration.fRtsControl = RTS_CONTROL_DISABLE;
        }

        configuration.fAbortOnError = FALSE;

        if (!SetCommState(handle_, &configuration))
        {
            return false;
        }

        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 500;

        return SetCommTimeouts(handle_, &timeouts) != FALSE;
    }

    void WindowsSerialTransport::close()
    {
        if (!isOpen())
        {
            return;
        }

        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }

    bool WindowsSerialTransport::isOpen() const
    {
        return handle_ != INVALID_HANDLE_VALUE;
    }

    std::size_t WindowsSerialTransport::read(
        std::span<std::byte> buffer)
    {
        if (!isOpen() || buffer.empty())
        {
            return 0;
        }

        const DWORD requested =
            static_cast<DWORD>(
                std::min<std::size_t>(
                    buffer.size(),
                    std::numeric_limits<DWORD>::max()));

        DWORD bytesRead = 0;

        if (!ReadFile(
            handle_,
            buffer.data(),
            requested,
            &bytesRead,
            nullptr))
        {
            return 0;
        }

        return static_cast<std::size_t>(bytesRead);
    }

    std::size_t WindowsSerialTransport::write(
        std::span<const std::byte> data)
    {
        if (!isOpen() || data.empty())
        {
            return 0;
        }

        const DWORD requested =
            static_cast<DWORD>(
                std::min<std::size_t>(
                    data.size(),
                    std::numeric_limits<DWORD>::max()));

        DWORD bytesWritten = 0;

        if (!WriteFile(
            handle_,
            data.data(),
            requested,
            &bytesWritten,
            nullptr))
        {
            return 0;
        }

        return static_cast<std::size_t>(bytesWritten);
    }

    const std::string&
        WindowsSerialTransport::portName() const
    {
        return portName_;
    }
}
