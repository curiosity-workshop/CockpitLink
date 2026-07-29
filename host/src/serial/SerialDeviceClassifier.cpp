#include <cockpitlink/serial/SerialDeviceClassifier.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace cockpitlink::serial
{
    namespace
    {
        std::string toUpper(std::string value)
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(
                        std::toupper(character));
                });

            return value;
        }

        bool contains(
            const std::string& text,
            const std::string& value)
        {
            return text.find(value) != std::string::npos;
        }
    }

    SerialDeviceKind SerialDeviceClassifier::classify(
        const SerialPortInfo& port)
    {
        const std::string hardwareId =
            toUpper(port.hardwareId);

        const std::string displayName =
            toUpper(port.displayName);

        const std::string manufacturer =
            toUpper(port.manufacturer);

        if (contains(hardwareId, "BTHENUM\\") ||
            contains(displayName, "BLUETOOTH"))
        {
            return SerialDeviceKind::Bluetooth;
        }

        if (contains(hardwareId, "ACPI\\VEN_PNP&DEV_0501") ||
            contains(displayName, "COMMUNICATIONS PORT"))
        {
            return SerialDeviceKind::BuiltInSerial;
        }

        if (contains(hardwareId, "VID_2341") ||
            contains(hardwareId, "VID_2A03") ||
            contains(hardwareId, "VID_1A86") ||
            contains(hardwareId, "VID_10C4") ||
            contains(hardwareId, "VID_0403"))
        {
            return SerialDeviceKind::ArduinoCompatible;
        }

        if (contains(hardwareId, "USB\\") ||
            contains(displayName, "USB SERIAL") ||
            contains(manufacturer, "FTDI") ||
            contains(manufacturer, "SILICON LABS"))
        {
            return SerialDeviceKind::UsbSerial;
        }

        return SerialDeviceKind::Unknown;
    }
}
