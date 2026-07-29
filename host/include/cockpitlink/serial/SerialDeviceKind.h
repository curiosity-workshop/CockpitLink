#pragma once

namespace cockpitlink::serial
{
    enum class SerialDeviceKind
    {
        Unknown,
        ArduinoCompatible,
        UsbSerial,
        Bluetooth,
        BuiltInSerial
    };
}
