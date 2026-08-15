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
- `switchInput(pin).reversed().controls(behaviorId)`
- `button(pin).triggers(behaviorId)`
- `button(pin).startsEnds(behaviorId)`
- `button(pin).clicks(behaviorId)`
- `button(pin).doubleClicks(behaviorId)`
- `button(pin).longPresses(behaviorId)`
- `button(pin).debounce(milliseconds)`
- `button(pin).doubleClickWithin(milliseconds)`
- `button(pin).longPressAfter(milliseconds)`
- `encoder(pinA, pinB).changes(clockwiseBehaviorId, counterclockwiseBehaviorId)`
- `encoder(pinA, pinB).dividedBy(transitionsPerClick)`
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

## Fixed Memory Pools

CockpitLink does not allocate control state from the heap. Registrations are
placed into fixed, typed pools so a switch does not reserve the larger state
needed by a button gesture or analog axis. The defaults are:

| Setting | Default | Used by |
| --- | ---: | --- |
| `COCKPITLINK_MAX_REGISTRATIONS` | 64 | All behavior registrations combined |
| `COCKPITLINK_MAX_SWITCHES` | 32 | Maintained on/off switch inputs |
| `COCKPITLINK_MAX_BUTTONS` | 24 | Command buttons and momentary switches |
| `COCKPITLINK_MAX_OUTPUTS` | 16 | Digital outputs following simulator values |
| `COCKPITLINK_MAX_AXES` | 12 | Potentiometers and centered joystick axes |
| `COCKPITLINK_MAX_INTEGERS` | 8 | Integer values received from the simulator |
| `COCKPITLINK_MAX_ENCODERS` | 8 | Rotary encoder hardware instances |
| `COCKPITLINK_MAX_ENCODER_MODES` | 8 | Command-pair modes stored by each encoder |

A sketch can tune RAM use by defining limits before including the library:

```cpp
#define COCKPITLINK_MAX_REGISTRATIONS 40
#define COCKPITLINK_MAX_SWITCHES 24
#define COCKPITLINK_MAX_BUTTONS 12
#define COCKPITLINK_MAX_OUTPUTS 4
#define COCKPITLINK_MAX_AXES 9
#define COCKPITLINK_MAX_INTEGERS 4
#define COCKPITLINK_MAX_ENCODERS 2
#define COCKPITLINK_MAX_ENCODER_MODES 8
#include <CockpitLink.h>
```

Each limit must be at least one and no greater than 255. If a pool is full, a
later helper registration of that type is ignored while existing controls keep
running. `COCKPITLINK_MAX_REGISTRATIONS` must also cover the total number of
behavior registrations; each encoder consumes two because it has clockwise and
counterclockwise commands.

An encoder's initial `changes()` call returns its fixed pool index. Additional
command pairs can be registered once during `setup()` and selected locally at
runtime without reconnecting or allocating memory:

```cpp
uint8_t knob = CockpitLink.encoder(33, 34)
    .dividedBy(2)
    .changes("autopilot.heading_down", "autopilot.heading_up");

CockpitLink.addEncoderMode(knob,
    "autopilot.vertical_speed_down",
    "autopilot.vertical_speed_up");

CockpitLink.selectEncoderMode(knob, 1);
```

Changing mode clears queued steps from the previous mode so a fast turn cannot
spill commands into the newly selected function.

Context-sensitive local controls can register a command once and trigger its
assigned handle later:

```cpp
uint8_t sync = CockpitLink.registerCommand("autopilot.heading_sync");
// Later, after a debounced local button event:
CockpitLink.triggerCommand(sync);
```
