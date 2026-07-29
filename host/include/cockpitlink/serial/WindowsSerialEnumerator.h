#pragma once

#include <cockpitlink/serial/ISerialEnumerator.h>

namespace cockpitlink::serial
{
    class WindowsSerialEnumerator final : public ISerialEnumerator
    {
    public:
        std::vector<SerialPortInfo> enumerate() const override;
    };
}
