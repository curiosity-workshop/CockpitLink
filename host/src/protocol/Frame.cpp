#include <cockpitlink/protocol/Frame.h>

#include <algorithm>

namespace cockpitlink::protocol
{
    namespace
    {
        void pushU16(
            std::vector<std::byte>& bytes,
            std::uint16_t value)
        {
            bytes.push_back(
                static_cast<std::byte>((value >> 8) & 0xff));
            bytes.push_back(
                static_cast<std::byte>(value & 0xff));
        }

        void pushU32(
            std::vector<std::byte>& bytes,
            std::uint32_t value)
        {
            bytes.push_back(
                static_cast<std::byte>((value >> 24) & 0xff));
            bytes.push_back(
                static_cast<std::byte>((value >> 16) & 0xff));
            bytes.push_back(
                static_cast<std::byte>((value >> 8) & 0xff));
            bytes.push_back(
                static_cast<std::byte>(value & 0xff));
        }
    }

    std::uint32_t crc32(
        std::span<const std::byte> bytes)
    {
        std::uint32_t crc = 0xffffffffu;

        for (const std::byte value : bytes)
        {
            crc ^=
                static_cast<std::uint8_t>(value);

            for (int bit = 0; bit < 8; ++bit)
            {
                const std::uint32_t mask =
                    0u - (crc & 1u);
                crc =
                    (crc >> 1) ^ (0xedb88320u & mask);
            }
        }

        return ~crc;
    }

    std::vector<std::byte> encodeFrame(
        const Frame& frame)
    {
        const std::uint16_t payloadLength =
            static_cast<std::uint16_t>(
                std::min<std::size_t>(
                    frame.payload.size(),
                    maximumPayloadSize));

        std::vector<std::byte> bytes;
        bytes.reserve(
            headerSize +
            payloadLength +
            checksumSize);

        bytes.push_back(magic0);
        bytes.push_back(magic1);
        bytes.push_back(
            static_cast<std::byte>(protocolVersion));
        bytes.push_back(
            static_cast<std::byte>(frame.type));
        bytes.push_back(
            static_cast<std::byte>(frame.flags));
        pushU16(bytes, frame.sequence);
        pushU16(bytes, payloadLength);

        bytes.insert(
            bytes.end(),
            frame.payload.begin(),
            frame.payload.begin() + payloadLength);

        const std::uint32_t checksum =
            crc32(bytes);

        pushU32(bytes, checksum);

        return bytes;
    }
}
