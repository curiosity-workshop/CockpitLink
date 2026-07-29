#include <cockpitlink/protocol/FrameParser.h>

#include <algorithm>
#include <cstdint>

namespace cockpitlink::protocol
{
    namespace
    {
        std::uint16_t readU16(
            std::byte high,
            std::byte low)
        {
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(high)) << 8) |
                static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(low)));
        }

        std::uint32_t readU32(
            std::span<const std::byte> bytes)
        {
            std::uint32_t value = 0;

            for (const std::byte byte : bytes)
            {
                value =
                    (value << 8) |
                    std::to_integer<std::uint8_t>(byte);
            }

            return value;
        }
    }

    std::vector<Frame> FrameParser::push(
        std::span<const std::byte> bytes)
    {
        std::vector<Frame> frames;

        for (const std::byte byte : bytes)
        {
            switch (state_)
            {
            case State::SeekingMagic0:
                if (byte == magic0)
                {
                    header_.clear();
                    header_.push_back(byte);
                    state_ = State::SeekingMagic1;
                }
                break;

            case State::SeekingMagic1:
                if (byte == magic1)
                {
                    header_.push_back(byte);
                    state_ = State::ReadingHeader;
                }
                else
                {
                    resetFrame();
                    if (byte == magic0)
                    {
                        header_.push_back(byte);
                        state_ = State::SeekingMagic1;
                    }
                }
                break;

            case State::ReadingHeader:
                header_.push_back(byte);

                if (header_.size() != headerSize)
                {
                    break;
                }

                if (std::to_integer<std::uint8_t>(header_[2]) !=
                    protocolVersion)
                {
                    resetFrame();
                    break;
                }

                type_ =
                    static_cast<MessageType>(
                        std::to_integer<std::uint8_t>(header_[3]));
                flags_ =
                    std::to_integer<std::uint8_t>(header_[4]);
                sequence_ =
                    readU16(header_[5], header_[6]);
                payloadLength_ =
                    readU16(header_[7], header_[8]);

                if (payloadLength_ > maximumPayloadSize)
                {
                    resetFrame();
                    break;
                }

                payload_.clear();
                payload_.reserve(payloadLength_);
                state_ =
                    payloadLength_ == 0 ?
                        State::ReadingChecksum :
                        State::ReadingPayload;
                break;

            case State::ReadingPayload:
                payload_.push_back(byte);

                if (payload_.size() == payloadLength_)
                {
                    state_ = State::ReadingChecksum;
                }
                break;

            case State::ReadingChecksum:
                checksum_.push_back(byte);

                if (checksum_.size() == checksumSize)
                {
                    completeFrame(frames);
                    resetFrame();
                }
                break;
            }
        }

        return frames;
    }

    void FrameParser::reset()
    {
        resetFrame();
    }

    bool FrameParser::hasPartialFrame() const
    {
        return state_ != State::SeekingMagic0;
    }

    void FrameParser::resetFrame()
    {
        state_ = State::SeekingMagic0;
        header_.clear();
        payload_.clear();
        checksum_.clear();
        type_ = MessageType::Diagnostic;
        flags_ = 0;
        sequence_ = 0;
        payloadLength_ = 0;
    }

    bool FrameParser::completeFrame(
        std::vector<Frame>& frames)
    {
        std::vector<std::byte> covered;
        covered.reserve(
            header_.size() +
            payload_.size());
        covered.insert(
            covered.end(),
            header_.begin(),
            header_.end());
        covered.insert(
            covered.end(),
            payload_.begin(),
            payload_.end());

        if (crc32(covered) != readU32(checksum_))
        {
            return false;
        }

        frames.push_back({
            type_,
            flags_,
            sequence_,
            payload_
        });

        return true;
    }
}
