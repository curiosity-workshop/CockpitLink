#include <cockpitlink/transport/TransportSession.h>

#include <algorithm>
#include <span>

namespace cockpitlink::transport
{
    TransportSession::TransportSession(
        IByteTransport& transport,
        TransportSessionOptions options)
        : transport_{ transport },
        options_{ options }
    {
        if (options_.readBufferSize == 0)
        {
            options_.readBufferSize = 256;
        }

        if (options_.maximumReadPasses == 0)
        {
            options_.maximumReadPasses = 1;
        }

        if (options_.maximumMessagesPerTick == 0)
        {
            options_.maximumMessagesPerTick = 1;
        }

        if (options_.maximumWriteBytesPerTick == 0)
        {
            options_.maximumWriteBytesPerTick = 64;
        }
    }

    TransportSessionTickResult TransportSession::tick()
    {
        TransportSessionTickResult result;

        if (!transport_.isOpen())
        {
            return result;
        }

        pollIncoming(result);

        while (!inbound_.empty() &&
            result.frames.size() <
                options_.maximumMessagesPerTick)
        {
            result.frames.push_back(
                std::move(inbound_.front()));
            inbound_.pop_front();
        }

        flushOutput(result);
        result.coalescedFrames =
            totalCoalescedFrames_ -
            reportedCoalescedFrames_;
        reportedCoalescedFrames_ =
            totalCoalescedFrames_;
        return result;
    }

    void TransportSession::queueReliable(
        const protocol::Frame& frame)
    {
        reliable_.push_back(
            protocol::encodeFrame(frame));
    }

    void TransportSession::queueLatest(
        std::uint16_t key,
        const protocol::Frame& frame)
    {
        auto encoded = protocol::encodeFrame(frame);
        const auto found = streaming_.find(key);

        if (found == streaming_.end())
        {
            streamingOrder_.push_back(key);
            streaming_.emplace(key, std::move(encoded));
            return;
        }

        found->second = std::move(encoded);
        ++totalCoalescedFrames_;
    }

    void TransportSession::clear()
    {
        parser_.reset();
        inbound_.clear();
        reliable_.clear();
        streamingOrder_.clear();
        streaming_.clear();
        activeOutput_.clear();
        activeOutputOffset_ = 0;
        totalCoalescedFrames_ = 0;
        reportedCoalescedFrames_ = 0;
    }

    bool TransportSession::hasPendingOutput() const
    {
        return !activeOutput_.empty() ||
            !reliable_.empty() ||
            !streaming_.empty();
    }

    std::size_t TransportSession::reliableQueueSize() const
    {
        return reliable_.size();
    }

    std::size_t TransportSession::streamingQueueSize() const
    {
        return streaming_.size();
    }

    std::size_t TransportSession::totalCoalescedFrames() const
    {
        return totalCoalescedFrames_;
    }

    void TransportSession::pollIncoming(
        TransportSessionTickResult& result)
    {
        std::vector<std::byte> buffer(
            options_.readBufferSize);

        for (std::size_t pass = 0;
            pass < options_.maximumReadPasses;
            ++pass)
        {
            const auto bytesRead =
                transport_.read(buffer);

            if (bytesRead == 0)
            {
                break;
            }

            result.bytesRead += bytesRead;
            auto frames = parser_.push(
                std::span<const std::byte>{
                    buffer.data(),
                    bytesRead
                });

            for (auto& frame : frames)
            {
                inbound_.push_back(std::move(frame));
            }
        }
    }

    void TransportSession::selectNextOutput()
    {
        if (!activeOutput_.empty())
        {
            return;
        }

        if (!reliable_.empty())
        {
            activeOutput_ =
                std::move(reliable_.front());
            reliable_.pop_front();
            activeOutputOffset_ = 0;
            return;
        }

        while (!streamingOrder_.empty())
        {
            const auto key =
                streamingOrder_.front();
            streamingOrder_.pop_front();
            const auto found =
                streaming_.find(key);

            if (found == streaming_.end())
            {
                continue;
            }

            activeOutput_ =
                std::move(found->second);
            streaming_.erase(found);
            activeOutputOffset_ = 0;
            return;
        }
    }

    void TransportSession::flushOutput(
        TransportSessionTickResult& result)
    {
        selectNextOutput();

        if (activeOutput_.empty())
        {
            return;
        }

        const auto remaining =
            activeOutput_.size() -
            activeOutputOffset_;
        const auto requested =
            std::min(
                remaining,
                options_.maximumWriteBytesPerTick);
        const auto bytesWritten =
            transport_.write(
                std::span<const std::byte>{
                    activeOutput_.data() +
                        activeOutputOffset_,
                    requested
                });

        activeOutputOffset_ +=
            std::min(bytesWritten, requested);
        result.bytesWritten =
            std::min(bytesWritten, requested);

        if (activeOutputOffset_ >=
            activeOutput_.size())
        {
            activeOutput_.clear();
            activeOutputOffset_ = 0;
        }
    }
}
