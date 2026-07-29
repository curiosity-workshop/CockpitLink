#pragma once

#include <cockpitlink/protocol/Frame.h>

#include <cstddef>
#include <span>
#include <vector>

namespace cockpitlink::protocol
{
    class FrameParser
    {
    public:
        std::vector<Frame> push(
            std::span<const std::byte> bytes);

        void reset();
        bool hasPartialFrame() const;

    private:
        enum class State
        {
            SeekingMagic0,
            SeekingMagic1,
            ReadingHeader,
            ReadingPayload,
            ReadingChecksum
        };

        void resetFrame();
        bool completeFrame(
            std::vector<Frame>& frames);

        State state_ = State::SeekingMagic0;
        std::vector<std::byte> header_;
        std::vector<std::byte> payload_;
        std::vector<std::byte> checksum_;
        MessageType type_ = MessageType::Diagnostic;
        std::uint8_t flags_ = 0;
        std::uint16_t sequence_ = 0;
        std::uint16_t payloadLength_ = 0;
    };
}
