#pragma once

#include <cockpitlink/protocol/Frame.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>

namespace cockpitlink::logging
{
    enum class TraceDirection
    {
        Transmit,
        Receive
    };

    class SerialTraceLogger
    {
    public:
        explicit SerialTraceLogger(
            std::filesystem::path path);

        bool open();
        bool isOpen() const;
        const std::filesystem::path& path() const;

        void bytes(
            TraceDirection direction,
            std::string_view portName,
            std::span<const std::byte> bytes);

        void frame(
            TraceDirection direction,
            std::string_view portName,
            const protocol::Frame& frame);

        void message(
            std::string_view text);

    private:
        void writePrefix(
            TraceDirection direction,
            std::string_view portName,
            std::string_view kind);

        std::filesystem::path path_;
        std::ofstream stream_;
    };
}
