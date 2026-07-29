#pragma once

#include <cockpitlink/serial/SerialDeviceKind.h>
#include <cockpitlink/serial/SerialPortInfo.h>

namespace cockpitlink::serial
{
    class SerialDeviceClassifier
    {
    public:
        static SerialDeviceKind classify(
            const SerialPortInfo& port);
    };
}
