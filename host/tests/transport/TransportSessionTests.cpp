#include <cockpitlink/protocol/Frame.h>
#include <cockpitlink/protocol/FrameParser.h>
#include <cockpitlink/transport/TransportSession.h>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace
{
    class FakeTransport final
        : public cockpitlink::transport::IByteTransport
    {
    public:
        bool open() override
        {
            open_ = true;
            return true;
        }

        void close() override
        {
            open_ = false;
        }

        bool isOpen() const override
        {
            return open_;
        }

        std::size_t read(
            std::span<std::byte> buffer) override
        {
            if (incoming_.empty())
            {
                return 0;
            }

            auto& next = incoming_.front();
            const auto count =
                std::min(buffer.size(), next.size());
            std::copy_n(next.begin(), count, buffer.begin());
            next.erase(next.begin(), next.begin() + count);

            if (next.empty())
            {
                incoming_.pop_front();
            }

            return count;
        }

        std::size_t write(
            std::span<const std::byte> data) override
        {
            const auto count =
                std::min(data.size(), maximumWrite_);
            written_.insert(
                written_.end(),
                data.begin(),
                data.begin() + count);
            return count;
        }

        void pushIncoming(
            std::vector<std::byte> bytes)
        {
            incoming_.push_back(std::move(bytes));
        }

        bool open_ = true;
        std::size_t maximumWrite_ = 1024;
        std::deque<std::vector<std::byte>> incoming_;
        std::vector<std::byte> written_;
    };

    bool expect(
        bool condition,
        std::string_view message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
        }

        return condition;
    }
}

int main()
{
    using cockpitlink::protocol::Frame;
    using cockpitlink::protocol::FrameParser;
    using cockpitlink::protocol::MessageType;
    using cockpitlink::transport::TransportSession;
    using cockpitlink::transport::TransportSessionOptions;

    bool passed = true;
    FakeTransport transport;
    transport.maximumWrite_ = 5;
    TransportSession session{
        transport,
        TransportSessionOptions{
            .readBufferSize = 7,
            .maximumReadPasses = 2,
            .maximumMessagesPerTick = 1,
            .maximumWriteBytesPerTick = 8
        }
    };

    const Frame first{
        MessageType::Diagnostic,
        0,
        1,
        { std::byte{ 1 }, std::byte{ 2 } }
    };
    const Frame second{
        MessageType::Diagnostic,
        0,
        2,
        { std::byte{ 3 } }
    };

    auto incoming = cockpitlink::protocol::encodeFrame(first);
    const auto secondBytes =
        cockpitlink::protocol::encodeFrame(second);
    incoming.insert(
        incoming.end(),
        secondBytes.begin(),
        secondBytes.end());
    transport.pushIncoming(std::move(incoming));

    auto tick = session.tick();
    passed &= expect(
        tick.frames.empty(),
        "partial bounded reads should not invent a frame");
    tick = session.tick();
    passed &= expect(
        tick.frames.size() == 1 &&
            tick.frames[0].sequence == 1,
        "first inbound frame should be delivered");
    tick = session.tick();
    passed &= expect(
        tick.frames.size() == 1 &&
            tick.frames[0].sequence == 2,
        "message budget should defer the second frame");

    session.queueReliable(first);
    session.queueLatest(7, first);
    session.queueLatest(7, second);
    passed &= expect(
        session.totalCoalescedFrames() == 1 &&
            session.streamingQueueSize() == 1,
        "latest-value queue should coalesce by key");

    while (session.hasPendingOutput())
    {
        tick = session.tick();
        passed &= expect(
            tick.bytesWritten <= 5,
            "partial transport write should be retained");
    }

    FrameParser parser;
    const auto writtenFrames =
        parser.push(transport.written_);
    passed &= expect(
        writtenFrames.size() == 2,
        "reliable and coalesced frames should both be written");
    passed &= expect(
        writtenFrames.size() == 2 &&
            writtenFrames[0].sequence == 1 &&
            writtenFrames[1].sequence == 2,
        "reliable priority or latest-value ordering failed");

    return passed ? 0 : 1;
}
