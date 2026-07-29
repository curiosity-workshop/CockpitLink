#pragma once

#include <cstddef>
#include <span>

namespace cockpitlink::transport
{
    class IByteTransport
    {
    public:
        virtual ~IByteTransport() = default;

        virtual bool open() = 0;
        virtual void close() = 0;
        virtual bool isOpen() const = 0;

        virtual std::size_t read(
            std::span<std::byte> buffer) = 0;

        virtual std::size_t write(
            std::span<const std::byte> data) = 0;
    };
}
