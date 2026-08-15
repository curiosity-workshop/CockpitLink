#include <Windows.h>
#include <SimConnect.h>

#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <string>

namespace
{
    enum class DataDefinition : DWORD
    {
        AircraftIdentity = 1
    };

    enum class DataRequest : DWORD
    {
        AircraftIdentity = 1,
        InputEvents = 2
    };

    enum class SystemEvent : DWORD
    {
        SimStart = 1,
        SimStop = 2,
        AircraftLoaded = 3
    };

    struct AircraftIdentity
    {
        char title[256]{};
    };

    struct ProbeState
    {
        HANDLE connection = nullptr;
        bool opened = false;
        bool aircraftReceived = false;
        bool inputEventsReceived = false;
        bool quit = false;
        bool exception = false;
    };

    bool requestAircraftIdentity(HANDLE connection)
    {
        const HRESULT defineResult = SimConnect_AddToDataDefinition(
            connection,
            static_cast<DWORD>(DataDefinition::AircraftIdentity),
            "TITLE",
            nullptr,
            SIMCONNECT_DATATYPE_STRING256);
        if (FAILED(defineResult))
        {
            return false;
        }

        return SUCCEEDED(SimConnect_RequestDataOnSimObject(
            connection,
            static_cast<DWORD>(DataRequest::AircraftIdentity),
            static_cast<DWORD>(DataDefinition::AircraftIdentity),
            SIMCONNECT_OBJECT_ID_USER,
            SIMCONNECT_PERIOD_ONCE));
    }

    void CALLBACK dispatch(
        SIMCONNECT_RECV* message,
        DWORD,
        void* context)
    {
        auto& state = *static_cast<ProbeState*>(context);

        switch (message->dwID)
        {
        case SIMCONNECT_RECV_ID_OPEN:
        {
            const auto& opened =
                *static_cast<SIMCONNECT_RECV_OPEN*>(message);
            std::cout
                << "Connected to " << opened.szApplicationName << '\n'
                << "Simulator version "
                << opened.dwApplicationVersionMajor << '.'
                << opened.dwApplicationVersionMinor << " build "
                << opened.dwApplicationBuildMajor << '.'
                << opened.dwApplicationBuildMinor << '\n'
                << "SimConnect version "
                << opened.dwSimConnectVersionMajor << '.'
                << opened.dwSimConnectVersionMinor << " build "
                << opened.dwSimConnectBuildMajor << '.'
                << opened.dwSimConnectBuildMinor << '\n';
            state.opened = true;
            SimConnect_SubscribeToSystemEvent(
                state.connection,
                static_cast<DWORD>(SystemEvent::SimStart),
                "SimStart");
            SimConnect_SubscribeToSystemEvent(
                state.connection,
                static_cast<DWORD>(SystemEvent::SimStop),
                "SimStop");
            SimConnect_SubscribeToSystemEvent(
                state.connection,
                static_cast<DWORD>(SystemEvent::AircraftLoaded),
                "AircraftLoaded");
            if (!requestAircraftIdentity(state.connection))
            {
                std::cerr << "Could not request the loaded aircraft title.\n";
                state.exception = true;
            }
            if (FAILED(SimConnect_EnumerateInputEvents(
                state.connection,
                static_cast<DWORD>(DataRequest::InputEvents))))
            {
                std::cerr << "Could not enumerate aircraft Input Events.\n";
                state.exception = true;
            }
            break;
        }
        case SIMCONNECT_RECV_ID_ENUMERATE_INPUT_EVENTS:
        {
            const auto& events =
                *static_cast<SIMCONNECT_RECV_ENUMERATE_INPUT_EVENTS*>(message);
            for (DWORD index = 0; index < events.dwArraySize; ++index)
            {
                const auto& event = events.rgData[index];
                std::string lower{ event.Name };
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char character)
                    {
                        return static_cast<char>(std::tolower(character));
                    });
                if (lower.find("condition") != std::string::npos ||
                    lower.find("mixture") != std::string::npos ||
                    lower.find("fuel") != std::string::npos ||
                    lower.find("cutoff") != std::string::npos ||
                    lower.find("idle") != std::string::npos)
                {
                    std::cout << "Input Event: " << event.Name
                        << " hash=" << event.Hash
                        << " type=" << event.eType << '\n';
                }
            }
            if (events.dwEntryNumber + 1 >= events.dwOutOf)
            {
                state.inputEventsReceived = true;
            }
            break;
        }
        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
        {
            const auto& data =
                *static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message);
            if (data.dwRequestID ==
                static_cast<DWORD>(DataRequest::AircraftIdentity))
            {
                const auto& aircraft =
                    *reinterpret_cast<const AircraftIdentity*>(&data.dwData);
                std::cout << "Aircraft title: " << aircraft.title << '\n';
                state.aircraftReceived = true;
            }
            break;
        }
        case SIMCONNECT_RECV_ID_EVENT_FILENAME:
        {
            const auto& event =
                *static_cast<SIMCONNECT_RECV_EVENT_FILENAME*>(message);
            if (event.uEventID ==
                static_cast<DWORD>(SystemEvent::AircraftLoaded))
            {
                std::cout << "Aircraft configuration: "
                    << event.szFileName << '\n';
                requestAircraftIdentity(state.connection);
            }
            break;
        }
        case SIMCONNECT_RECV_ID_EXCEPTION:
        {
            const auto& error =
                *static_cast<SIMCONNECT_RECV_EXCEPTION*>(message);
            std::cerr
                << "SimConnect exception " << error.dwException
                << ", send ID " << error.dwSendID
                << ", parameter " << error.dwIndex << '\n';
            state.exception = true;
            break;
        }
        case SIMCONNECT_RECV_ID_QUIT:
            std::cout << "Microsoft Flight Simulator is shutting down.\n";
            state.quit = true;
            break;
        default:
            break;
        }
    }
}

int main()
{
    HANDLE connection = nullptr;
    const HRESULT result = SimConnect_Open(
        &connection,
        "CockpitLink MSFS Probe",
        nullptr,
        0,
        nullptr,
        0);

    if (FAILED(result))
    {
        std::cerr
            << "SimConnect_Open failed (HRESULT 0x"
            << std::hex << std::uppercase
            << static_cast<unsigned long>(result)
            << "). Start MSFS and enter a flight, then run the probe again.\n";
        return 2;
    }

    ProbeState state;
    state.connection = connection;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);

    while ((!state.aircraftReceived || !state.inputEventsReceived) &&
        !state.quit && !state.exception &&
        std::chrono::steady_clock::now() < deadline)
    {
        const HRESULT dispatchResult =
            SimConnect_CallDispatch(connection, dispatch, &state);
        if (FAILED(dispatchResult))
        {
            std::cerr << "Dispatching SimConnect messages failed.\n";
            break;
        }
        Sleep(100);
    }

    if (!state.aircraftReceived && !state.quit && !state.exception)
    {
        std::cerr << "Timed out waiting for the aircraft identity.\n";
    }

    SimConnect_Close(connection);
    return state.opened && state.aircraftReceived &&
        state.inputEventsReceived ? 0 : 3;
}
