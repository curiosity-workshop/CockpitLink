#include <cockpitlink/catalog/BehaviorCatalog.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
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

int main(
    int argc,
    char** argv)
{
    if (argc != 4)
    {
        std::cerr
            << "usage: BehaviorCatalogValidationTests <catalog.json> "
               "<aircraft-profile.json> <user-profile.json>\n";
        return 1;
    }

    std::vector<std::string> errors;
    const auto catalog =
        cockpitlink::catalog::loadBehaviorCatalog(argv[1], errors);
    bool passed = true;

    for (const auto& error : errors)
    {
        std::cerr << error << '\n';
    }

    passed &= expect(
        catalog.has_value(),
        "base catalog should load");

    if (!catalog)
    {
        return 1;
    }

    passed &= expect(
        catalog->version() == 1,
        "catalog version should be parsed");
    passed &= expect(
        catalog->name() == "CockpitLink Base Behaviors",
        "catalog name should be parsed");
    passed &= expect(
        catalog->behaviors().size() == 64,
        "all base behaviors should be parsed");

    const auto* beacon = catalog->find("lights.beacon");
    passed &= expect(
        beacon != nullptr,
        "lights.beacon should resolve");
    passed &= expect(
        beacon &&
            beacon->valueType ==
                cockpitlink::catalog::ValueType::Boolean,
        "lights.beacon should be boolean");
    passed &= expect(
        beacon && beacon->xplane &&
            beacon->xplane->read &&
            beacon->xplane->read->dataRef ==
                "sim/cockpit/electrical/beacon_lights_on",
        "beacon X-Plane read dataref should resolve");
    passed &= expect(
        beacon && beacon->xplane &&
            beacon->xplane->writeStrategy ==
                cockpitlink::catalog::WriteStrategy::
                    SetViaToggleWhenKnown,
        "beacon write strategy should resolve");
    passed &= expect(
        beacon && beacon->msfs && beacon->msfs->read &&
            beacon->msfs->read->simVar == "LIGHT BEACON" &&
            beacon->msfs->event &&
            beacon->msfs->event->event == "BEACON_LIGHTS_SET",
        "beacon MSFS SimVar and event should resolve");

    const auto* throttle = catalog->find("engine.1.throttle");
    passed &= expect(
        throttle && throttle->rangeMinimum == 0.0 &&
            throttle->rangeMaximum == 100.0,
        "throttle canonical range should resolve");
    passed &= expect(
        throttle && throttle->xplane &&
            throttle->xplane->write &&
            throttle->xplane->write->index == 0,
        "throttle array index should resolve");
    passed &= expect(
        throttle && throttle->xplane &&
            throttle->xplane->write &&
            throttle->xplane->write->scale &&
            throttle->xplane->write->scale->toMax == 1.0,
        "throttle write scale should resolve");

    const auto handle = catalog->handleFor("flight.roll");
    passed &= expect(
        handle && catalog->atHandle(*handle) &&
            catalog->atHandle(*handle)->id == "flight.roll",
        "catalog handles should round-trip");
    const auto* roll = catalog->find("flight.roll");
    passed &= expect(
        roll && roll->msfs && roll->msfs->event &&
            roll->msfs->event->event == "AXIS_AILERONS_SET" &&
            roll->msfs->event->scale &&
            roll->msfs->event->scale->toMin == 16383.0 &&
            roll->msfs->event->scale->toMax == -16383.0,
        "roll MSFS event scaling should resolve");
    passed &= expect(
        !catalog->handleFor("missing.behavior"),
        "unknown behaviors should not get handles");

    const auto* trim = catalog->find("flight.elevator_trim_up");
    passed &= expect(
        trim && trim->xplane && trim->xplane->command &&
            *trim->xplane->command ==
                "sim/flight_controls/pitch_trim_up",
        "command binding should be parsed");

    errors.clear();
    const auto layered =
        cockpitlink::catalog::loadLayeredBehaviorCatalog(
            { argv[1], argv[2], argv[3] }, errors);
    for (const auto& error : errors)
    {
        std::cerr << error << '\n';
    }
    passed &= expect(
        layered.has_value(),
        "ordered aircraft and user profiles should load");
    if (layered)
    {
        const auto* kingAirThrottle =
            layered->find("engine.1.throttle");
        passed &= expect(
            kingAirThrottle && kingAirThrottle->msfs &&
                kingAirThrottle->msfs->event &&
                kingAirThrottle->msfs->event->scale &&
                kingAirThrottle->msfs->event->scale->toMin == -16383.0 &&
                kingAirThrottle->msfs->event->scale->toMax == 16383.0,
            "King Air throttle profile should cover the signed axis range");
        const auto* condition1 = layered->find("engine.1.mixture");
        passed &= expect(
            condition1 && condition1->msfs &&
                condition1->msfs->inputEvent &&
                condition1->msfs->inputEvent->name ==
                    "FUEL_1_Condition_Lever" &&
                condition1->msfs->inputEvent->scale &&
                condition1->msfs->inputEvent->scale->toMin == 2.0 &&
                condition1->msfs->inputEvent->scale->toMax == 0.0 &&
                condition1->msfs->inputEvent->steps == 3,
            "aircraft profile should override the engine 1 fuel lever");
        passed &= expect(
            condition1 && condition1->msfsSource.find(
                "msfs-king-air-350i.json") != std::string::npos,
            "binding provenance should identify the aircraft profile");
        const auto layeredRoll = layered->handleFor("flight.roll");
        passed &= expect(
            layeredRoll == handle,
            "catalog overlays must preserve protocol handles");
        const auto* yaw = layered->find("flight.yaw");
        passed &= expect(
            yaw && yaw->msfsSource.find("user-profile.json") !=
                std::string::npos,
            "the user layer should have highest binding precedence");
    }

    const auto invalidPath =
        std::filesystem::temp_directory_path() /
        "cockpitlink-invalid-catalog.json";
    {
        std::ofstream invalid{ invalidPath };
        invalid << R"({"catalogVersion":1,"name":"bad","behaviors":[)"
                   R"({"id":"Not Valid"}]})";
    }

    errors.clear();
    passed &= expect(
        !cockpitlink::catalog::loadBehaviorCatalog(
            invalidPath,
            errors),
        "invalid catalogs should be rejected");
    passed &= expect(
        !errors.empty(),
        "invalid catalogs should return diagnostics");

    std::error_code removeError;
    std::filesystem::remove(invalidPath, removeError);

    return passed ? 0 : 1;
}
