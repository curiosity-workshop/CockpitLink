#include <cockpitlink/catalog/BehaviorCatalog.h>

#include <filesystem>
#include <fstream>
#include <iostream>
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
    if (argc != 2)
    {
        std::cerr
            << "usage: BehaviorCatalogValidationTests <catalog.json>\n";
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
        catalog->behaviors().size() == 14,
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
    passed &= expect(
        !catalog->handleFor("missing.behavior"),
        "unknown behaviors should not get handles");

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
