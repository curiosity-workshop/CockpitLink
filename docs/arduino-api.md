# Arduino API Concept

The Arduino API should keep the common path behavior-centered.

## Minimal Example

```cpp
#include <CockpitLink.h>

void setup()
{
    CockpitLink.begin("Beacon Panel", "2026.07.20");

    CockpitLink.switchInput(7)
        .controls("lights.beacon");

    CockpitLink.digitalOutput(LED_BUILTIN)
        .follows("lights.beacon");
}

void loop()
{
    CockpitLink.loop();
}
```

## Callback Example

```cpp
CockpitLink.behavior("autopilot.altitude_select")
    .updatesEvery(100)
    .bucket(100)
    .onChange([](float altitude) {
        displayAltitude(altitude);
    });
```

## Direct Escape Hatch

Advanced users should still be able to bind directly:

```cpp
auto beacon =
    CockpitLink.xplaneDataRef<int>(
        "sim/cockpit/electrical/beacon_lights_on");

auto toggle =
    CockpitLink.xplaneCommand(
        "sim/lights/beacon_lights_toggle");
```

## Safer Data Values

Data values must use pointer plus length:

```cpp
CockpitLink.behavior("aircraft.custom.payload")
    .onData([](const uint8_t* data, size_t length) {
        handlePayload(data, length);
    });
```

## Desired Helpers

- `switchInput(pin).controls(behaviorId)`
- `button(pin).triggers(behaviorId)`
- `button(pin).startsEnds(behaviorId)`
- `potentiometer(pin).controls(behaviorId)`
- `potentiometer(pin).calibrated(rawMin, rawMax)`
- `potentiometer(pin).deadband(percent)`
- `potentiometer(pin).bucket(percent)`
- `potentiometer(pin).sampleEvery(milliseconds)`
- `potentiometer(pin).readPercent()`
- `joystickAxis(pin).centered(rawMin, rawCenter, rawMax)`
- `joystickAxis(pin).reversed()`
- `joystickAxis(pin).deadband(percent)`
- `joystickAxis(pin).bucket(percent)`
- `joystickAxis(pin).expo(percent)`
- `joystickAxis(pin).sampleEvery(milliseconds)`
- `joystickAxis(pin).controls(behaviorId)`
- `joystickAxis(pin).readCenteredPercent()`
- `digitalOutput(pin).follows(behaviorId)`
- `encoder(pinA, pinB).changes(behaviorId, step)`
- `display(lcd).line(index).shows(behaviorId)`
- `onConnected(callback)`
- `onDisconnected(callback)`
- `debug(message)`
- `debugValue(label, value)`

## Capability-Aware Behavior

The user-facing API should make unavailable simulator abilities visible without
making sketches fragile.

Examples:

```cpp
auto beacon = CockpitLink.behavior("lights.beacon");

if (beacon.canWrite()) {
    CockpitLink.switchInput(7).controls(beacon);
}

CockpitLink.digitalOutput(LED_BUILTIN).follows(beacon);
```

For common sketches, helpers can report errors through diagnostics and keep the
rest of the device running.
