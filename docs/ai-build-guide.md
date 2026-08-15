# AI-Assisted CockpitLink Build Guide

This guide defines how a cockpit builder and an AI coding assistant collaborate
to build, bind, test, and preserve CockpitLink hardware. The objective is not
for AI to guess simulator internals. The objective is a short evidence loop:
describe intent, create the narrowest binding, validate it automatically, test
it in the simulator, and record the result.

## 1. Know the product boundaries

CockpitLink separates four concerns:

1. Firmware describes physical devices and requests semantic behaviors.
2. The base catalog defines stable cockpit meanings and canonical value types.
3. Simulator adapters execute bindings without exposing transport details to
   firmware.
4. Profiles override bindings for a simulator, aircraft, device, or user.

Normal development must not edit `catalog/base-behaviors.json`. The base is a
versioned product contract maintained through reviewed releases. A base change
is appropriate only when a reusable semantic behavior is genuinely missing or
a broadly valid binding is demonstrably incorrect. Aircraft quirks, wiring
direction, calibration, and experiments belong in profiles.

Profile precedence is fixed:

```text
base → simulator → aircraft → device → user
```

Later layers override bindings only. They cannot change a behavior's semantic
ID, kind, value type, canonical range, or protocol handle.

Tracked locations:

- `profiles/aircraft/`: validated, shareable aircraft differences.
- `profiles/templates/`: examples copied by users.
- `docs/`: product and testing documentation.

Private location:

- `profiles/local/`: Git-ignored user experiments and calibration.

## 2. Start with a hardware inventory

Give the AI enough physical evidence to avoid assumptions:

```text
Board: Arduino Mega 2560
Control: momentary ON-OFF-ON switch
Pins: D27 and D28, common to GND
Electrical mode: INPUT_PULLUP, active-low
Intended behavior: elevator trim down/up
Held behavior: move slowly until released
Simulator/aircraft: MSFS King Air 350i
```

For analog inputs include raw minimum, center, and maximum. For encoders include
pins, detents per revolution, electrical transitions per detent, direction,
button pin, and desired acceleration. For outputs include voltage, active
polarity, display size/address, and required update rate.

Update the wiring document when the reference device changes. The current
ButtonBox wiring is in `docs/buttonbox-wiring.md`.

## 3. Choose semantic behavior IDs

Firmware should express cockpit meaning:

```cpp
CockpitLink.button(25).triggers("flight.flaps_up");
CockpitLink.button(28).startsEnds("flight.elevator_trim_up");
CockpitLink.potentiometer(A4).controls("engine.1.throttle");
```

Do not put implementation details in an ID:

```text
Bad:  msfs.axis_throttle1_set
Bad:  xplane.sim_cockpit2_controls_yoke_pitch_ratio
Good: engine.1.throttle
Good: flight.pitch
```

Before adding an ID, search the base catalog. Reuse an existing ID only when
its meaning, value type, range, and direction model match. Do not reuse
`engine.1.mixture` for a fundamentally different control merely because the
same physical pot is available; use an aircraft binding or propose a new
semantic behavior when the distinction matters to device users.

## 4. Select the narrowest profile layer

Use a simulator profile for behavior shared by essentially all aircraft in one
simulator. Use an aircraft profile for module-specific events, ranges, detents,
or avionics. Use a device profile for a published hardware design. Use a local
user profile for calibration, direction, personal choices, and experiments.

Create local work by copying the template:

```powershell
New-Item -ItemType Directory -Force profiles/local
Copy-Item profiles/templates/user-profile.json profiles/local/my-controls.json
```

The host automatically appends `.json` files from `profiles/local/` in sorted
filename order. Prefix filenames when order matters:

```text
10-device-calibration.json
20-my-aircraft-preferences.json
```

Invalid local profiles stop startup with diagnostics. CockpitLink never
silently skips a malformed override.

## 5. Choose a simulator mechanism

### X-Plane

Prefer, in order:

1. Direct readable/writable dataref for persistent switch or axis state.
2. Explicit on/off commands.
3. A toggle command only when current state is readable.
4. A momentary command for actions that do not represent persistent state.

Record dataref type, array index, writable status, canonical scaling, and
whether a command supports trigger or begin/end semantics.

### MSFS

Prefer broadly supported key events or SimVars for general aircraft. Use
aircraft Input Events when the module ignores standard events or implements a
custom cockpit. `CockpitLinkMSFSProbe.exe` enumerates relevant live Input Event
names and reports the exact aircraft title.

For numeric Input Events, determine whether the control expects a continuous
range or discrete states. Use `steps` for stable discrete zones:

```json
"inputEvent": {
  "name": "FUEL_1_Condition_Lever",
  "steps": 3,
  "scale": {
    "fromMin": 0,
    "fromMax": 100,
    "toMin": 2,
    "toMax": 0
  }
}
```

An accepted SimConnect call is not proof that an aircraft acted on it. Observe
the cockpit control and, when possible, read state back.

### Future adapters

DCS and other simulators follow the same semantic/profile model. Simulator
transport, lifecycle, and command execution belong in adapters; aircraft
device/command identifiers belong in profiles.

## 6. Express direction, scaling, and detents

Canonical values face firmware and users. Simulator values belong in bindings.
For example, a physical throttle remains `0..100` even if an aircraft expects a
signed simulator axis:

```json
"scale": {
  "fromMin": 0,
  "fromMax": 100,
  "toMin": -16383,
  "toMax": 16383
}
```

Reverse a profile by swapping `toMin` and `toMax`; do not reverse an unrelated
simulator binding. Calibrate with observed endpoints rather than nominal ADC
limits when the mechanism cannot reach full travel.

Treat gated ranges such as turboprop reverse, ground fine, flight idle, low
idle, and cutoff as aircraft behavior. Do not expose a hazardous or unintended
range through an ungated physical pot without an explicit profile decision.

## 7. Moderate commands and serial flow

The transport is bounded and must stay nonblocking. Do not add blocking serial
reads, long sleeps, or unbounded registration bursts. The host owns simulator
command scheduling; firmware owns physical sampling and gesture recognition.

Use:

- trigger for one-shot actions such as one flap notch;
- begin/end for controls that remain active while held;
- host-side moderated repeat where a simulator only offers trigger events;
- encoder accumulation and bounded draining for fast rotation;
- coalesced streaming updates for replaceable axis data.

Rate and acceleration changes require live testing. A control that feels good
at one frame rate must not flood a slower simulation loop.

## 8. Build and validate

Run builds from a Visual Studio developer environment. The configured MSFS SDK
is auto-detected from `C:/MSFS SDK` or `COCKPITLINK_MSFS_SDK_PATH`.

```powershell
cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/x64-Debug --target `
  CockpitLinkMSFS CockpitLinkMSFSProbe `
  CockpitLinkCatalogTests CockpitLinkProtocolTests CockpitLinkTransportTests
ctest --test-dir out/build/x64-Debug --output-on-failure
git diff --check
```

Build the Arduino example using the board and library versions intended for
distribution. Confirm flash/RAM use and mirror the validated library and sketch
to the user's Arduino sketchbook only when explicitly requested.

Required automated checks:

- JSON/schema parsing succeeds.
- Layer precedence and provenance are correct.
- Protocol handles remain stable after overlays.
- Protocol and frame-parser tests pass.
- Transport tests pass.
- The affected simulator adapter builds.

## 9. Perform the live test

Test one control family at a time:

1. Confirm simulator, exact aircraft, catalog layers, firmware, and COM port.
2. Move or press one control slowly.
3. Verify direction, complete range, center/detents, and feedback.
4. Test a short press/click.
5. Test a held input.
6. Test fast encoder movement where applicable.
7. Disconnect/reconnect the device.
8. Reload or shut down the simulator.
9. Confirm no command remains held and no axis jumps unexpectedly.

Use `docs/diagnostic-report.md` to preserve the result. Pilot observations are
valuable evidence. Translate them into profile behavior without discarding the
original description.

## 10. Promote a local profile

A local override may become a tracked aircraft or device profile only after:

- exact simulator and aircraft identity are recorded;
- direction, range, detents, held behavior, and feedback are tested;
- reconnect and shutdown behavior are checked;
- catalog and transport tests pass;
- known limitations are documented;
- private paths and machine-specific values are removed.

Promotion does not imply a base-catalog change. Copy only the validated,
shareable overrides into the appropriate tracked profile.

## 11. Worked example: MSFS King Air 350i

The reference ButtonBox exposed A8/A9 as generic mixture controls. Standard
MSFS condition-lever key events were accepted but produced no movement in the
stock King Air. The live probe reported:

```text
Aircraft title: Beechcraft King Air 350I Kenmore Livery
Input Event: FUEL_1_Condition_Lever
Input Event: FUEL_2_Condition_Lever
```

The aircraft profile therefore overrides only the two MSFS bindings. Live
testing then established that:

- the Input Events worked;
- physical direction was reversed;
- continuous fractional values could skip stable low idle;
- three exact reversed states produced high idle, low idle, and cutoff;
- King Air throttle events required a signed range to reach flight idle;
- ground fine and reverse remained intentionally unsupported by the ungated
  reference pots.

Those findings live in
`profiles/aircraft/msfs-king-air-350i.json`. The base catalog and X-Plane
bindings remain unchanged. Successful flight testing included KASE to KCOS,
with ground fine/reverse documented as deferred rather than guessed.

## 12. Reusable AI prompts

### Create a local profile

```text
Read AGENTS.md, docs/ai-build-guide.md, and docs/behavior-catalog.md.
Do not edit the base catalog. My hardware and diagnostic report follow:
[paste inventory and report]
Create the narrowest profiles/local override, validate it, and explain the
exact live-test sequence. Do not claim success until I report simulator motion.
```

### Diagnose a control

```text
Use docs/diagnostic-report.md. This control resolves but behaves as follows:
[paste observations and relevant log lines]
Determine whether the fault is wiring, firmware gesture handling, transport,
generic simulator binding, or aircraft-specific behavior. Diagnose first;
change files only if I ask for the fix.
```

### Promote a validated profile

```text
This profiles/local override passed the attached acceptance report.
Review it against docs/ai-build-guide.md, remove private calibration unless it
is intrinsic to the published device, add regression assertions, and promote
only the shareable aircraft/device bindings. Leave the base catalog unchanged.
```

### Request a new semantic behavior

```text
No existing behavior ID represents this cockpit meaning: [description].
Review the base schema and existing IDs. Explain why an aircraft/profile-only
binding is insufficient, propose a simulator-neutral ID and canonical type,
range, units, and capabilities, and identify compatibility tests required for
a reviewed base-catalog addition.
```

## 13. Definition of done

A CockpitLink AI-assisted change is complete when the intended layer contains
the smallest correct change, automated validation passes, live behavior is
observed, diagnostics record the result, reconnect/shutdown are safe, and no
private profile or generated artifact is staged for Git.
