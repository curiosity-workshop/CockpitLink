#include <cockpitlink/logging/SerialTraceLogger.h>

#include <cockpitlink/protocol/TraceFormatter.h>

#include <chrono>
#include <iomanip>
#include <sstream>

namespace cockpitlink::logging
{
    namespace
    {
        const char* directionName(
            TraceDirection direction)
        {
            return direction == TraceDirection::Transmit ?
                "TX" :
                "RX";
        }

        std::string timestamp()
        {
            const auto now =
                std::chrono::system_clock::now();
            const std::time_t time =
                std::chrono::system_clock::to_time_t(now);

            std::tm localTime{};
#ifdef _WIN32
            localtime_s(&localTime, &time);
#else
            localtime_r(&time, &localTime);
#endif

            std::ostringstream text;
            text
                << std::put_time(
                    &localTime,
                    "%Y-%m-%d %H:%M:%S");
            return text.str();
        }
    }

    SerialTraceLogger::SerialTraceLogger(
        std::filesystem::path path)
        : path_(std::move(path))
    {
    }

    bool SerialTraceLogger::open()
    {
        if (stream_.is_open())
        {
            return true;
        }

        if (path_.has_parent_path())
        {
            std::filesystem::create_directories(
                path_.parent_path());
        }

        stream_.open(
            path_,
            std::ios::out | std::ios::app);

        if (stream_.is_open())
        {
            message("trace opened");
        }

        return stream_.is_open();
    }

    bool SerialTraceLogger::isOpen() const
    {
        return stream_.is_open();
    }

    const std::filesystem::path& SerialTraceLogger::path() const
    {
        return path_;
    }

    void SerialTraceLogger::bytes(
        TraceDirection direction,
        std::string_view portName,
        std::span<const std::byte> bytes)
    {
        if (!stream_.is_open())
        {
            return;
        }

        writePrefix(direction, portName, "raw");

        stream_
            << std::hex
            << std::setfill('0');

        for (const std::byte byte : bytes)
        {
            stream_
                << ' '
                << std::setw(2)
                << static_cast<int>(
                    std::to_integer<unsigned char>(byte));
        }

        stream_
            << std::dec
            << '\n';
        stream_.flush();
    }

    void SerialTraceLogger::frame(
        TraceDirection direction,
        std::string_view portName,
        const protocol::Frame& frame)
    {
        if (!stream_.is_open())
        {
            return;
        }

        writePrefix(direction, portName, "decoded");
        stream_
            << ' '
            << protocol::formatTraceLine(
                frame,
                direction == TraceDirection::Transmit)
            << '\n';
        stream_.flush();
    }

    void SerialTraceLogger::message(
        std::string_view text)
    {
        if (!stream_.is_open())
        {
            return;
        }

        stream_
            << timestamp()
            << " INFO "
            << text
            << '\n';
        stream_.flush();
    }

    void SerialTraceLogger::writePrefix(
        TraceDirection direction,
        std::string_view portName,
        std::string_view kind)
    {
        stream_
            << timestamp()
            << ' '
            << directionName(direction)
            << ' '
            << portName
            << ' '
            << kind
            << ':';
    }
}
