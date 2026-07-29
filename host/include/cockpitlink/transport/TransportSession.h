#pragma once

#include <cockpitlink/protocol/Frame.h>
#include <cockpitlink/protocol/FrameParser.h>
#include <cockpitlink/transport/IByteTransport.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <vector>

namespace cockpitlink::transport
{
    struct TransportSessionOptions
    {
        std::size_t readBufferSize = 256;
        std::size_t maximumReadPasses = 4;
        std::size_t maximumMessagesPerTick = 16;
        std::size_t maximumWriteBytesPerTick = 64;
    };

    struct TransportSessionTickResult
    {
        std::size_t bytesRead = 0;
        std::size_t bytesWritten = 0;
        std::size_t coalescedFrames = 0;
        std::vector<protocol::Frame> frames;
    };

    class TransportSession
    {
    public:
        explicit TransportSession(
            IByteTransport& transport,
            TransportSessionOptions options = {});

        TransportSessionTickResult tick();

        void queueReliable(
            const protocol::Frame& frame);

        void queueLatest(
            std::uint16_t key,
            const protocol::Frame& frame);

        void clear();

        bool hasPendingOutput() const;
        std::size_t reliableQueueSize() const;
        std::size_t streamingQueueSize() const;
        std::size_t totalCoalescedFrames() const;

    private:
        void pollIncoming(
            TransportSessionTickResult& result);

        void selectNextOutput();

        void flushOutput(
            TransportSessionTickResult& result);

        IByteTransport& transport_;
        TransportSessionOptions options_;
        protocol::FrameParser parser_;
        std::deque<protocol::Frame> inbound_;
        std::deque<std::vector<std::byte>> reliable_;
        std::deque<std::uint16_t> streamingOrder_;
        std::map<std::uint16_t, std::vector<std::byte>> streaming_;
        std::vector<std::byte> activeOutput_;
        std::size_t activeOutputOffset_ = 0;
        std::size_t totalCoalescedFrames_ = 0;
        std::size_t reportedCoalescedFrames_ = 0;
    };
}
