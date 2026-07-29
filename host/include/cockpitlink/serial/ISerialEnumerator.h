#pragma once

#include <cockpitlink/serial/SerialPortInfo.h>

#include <vector>

namespace cockpitlink::serial
{
    class ISerialEnumerator
    {
    public:
        virtual ~ISerialEnumerator() = default;

        virtual std::vector<SerialPortInfo> enumerate() const = 0;
    };
}
