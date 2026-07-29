#include <cockpitlink/serial/WindowsSerialEnumerator.h>

#include <cockpitlink/serial/SerialDeviceClassifier.h>

#include <Windows.h>
#include <SetupAPI.h>
#include <devguid.h>
#include <initguid.h>

#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "Setupapi.lib")

namespace cockpitlink::serial
{
    namespace
    {
        std::string getDeviceProperty(
            HDEVINFO deviceInfoSet,
            SP_DEVINFO_DATA& deviceInfo,
            DWORD property)
        {
            DWORD requiredSize = 0;

            SetupDiGetDeviceRegistryPropertyA(
                deviceInfoSet,
                &deviceInfo,
                property,
                nullptr,
                nullptr,
                0,
                &requiredSize);

            if (requiredSize == 0)
            {
                return {};
            }

            std::vector<BYTE> buffer(requiredSize);

            if (!SetupDiGetDeviceRegistryPropertyA(
                deviceInfoSet,
                &deviceInfo,
                property,
                nullptr,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                nullptr))
            {
                return {};
            }

            return reinterpret_cast<const char*>(buffer.data());
        }

        std::string getPortName(
            HDEVINFO deviceInfoSet,
            SP_DEVINFO_DATA& deviceInfo)
        {
            HKEY deviceKey = SetupDiOpenDevRegKey(
                deviceInfoSet,
                &deviceInfo,
                DICS_FLAG_GLOBAL,
                0,
                DIREG_DEV,
                KEY_READ);

            if (deviceKey == INVALID_HANDLE_VALUE)
            {
                return {};
            }

            char portName[256]{};
            DWORD valueType = 0;
            DWORD valueSize = sizeof(portName);

            const LONG result = RegQueryValueExA(
                deviceKey,
                "PortName",
                nullptr,
                &valueType,
                reinterpret_cast<BYTE*>(portName),
                &valueSize);

            RegCloseKey(deviceKey);

            if (result != ERROR_SUCCESS || valueType != REG_SZ)
            {
                return {};
            }

            return portName;
        }
    }

    std::vector<SerialPortInfo>
        WindowsSerialEnumerator::enumerate() const
    {
        std::vector<SerialPortInfo> ports;

        HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(
            &GUID_DEVCLASS_PORTS,
            nullptr,
            nullptr,
            DIGCF_PRESENT);

        if (deviceInfoSet == INVALID_HANDLE_VALUE)
        {
            return ports;
        }

        SP_DEVINFO_DATA deviceInfo{};
        deviceInfo.cbSize = sizeof(deviceInfo);

        for (DWORD index = 0;
            SetupDiEnumDeviceInfo(deviceInfoSet, index, &deviceInfo);
            ++index)
        {
            SerialPortInfo port;

            port.portName =
                getPortName(deviceInfoSet, deviceInfo);

            if (port.portName.empty())
            {
                continue;
            }

            port.displayName = getDeviceProperty(
                deviceInfoSet,
                deviceInfo,
                SPDRP_FRIENDLYNAME);

            port.hardwareId = getDeviceProperty(
                deviceInfoSet,
                deviceInfo,
                SPDRP_HARDWAREID);

            port.manufacturer = getDeviceProperty(
                deviceInfoSet,
                deviceInfo,
                SPDRP_MFG);

            port.kind =
                SerialDeviceClassifier::classify(port);

            ports.push_back(std::move(port));
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);

        return ports;
    }
}
