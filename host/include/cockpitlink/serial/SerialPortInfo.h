#pragma once

#include <cockpitlink/serial/SerialDeviceKind.h>

#include <string>

namespace cockpitlink::serial
{
    struct SerialPortInfo
    {
        std::string portName;
        std::string displayName;
        std::string manufacturer;
        std::string hardwareId;

        SerialDeviceKind kind = SerialDeviceKind::Unknown;
    };
}
