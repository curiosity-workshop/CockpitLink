# CockpitLink

CockpitLink is a scratch-built successor concept for cockpit hardware that
speaks in simulator behaviors instead of simulator-specific implementation
details.

Phoenix remains the X-Plane/XPLPro compatibility bridge. CockpitLink is the
clean-slate idea: a hardware API, behavior catalog, simulator adapters, and
transport protocol that can eventually support X-Plane, MSFS, DCS, or other
simulation targets.

## Goals

- Let device builders request cockpit behaviors such as `lights.beacon` or
  `autopilot.heading_bug` instead of raw datarefs, commands, or simvars.
- Keep direct simulator bindings available as an escape hatch.
- Use a binary transport protocol with decoded, human-readable logs.
- Support serial first, with IP transports designed in from the beginning.
- Allow aircraft-specific behavior overrides without changing Arduino sketches.
- Make device firmware stable across simulator sessions and simulator platforms.

## Project Shape

- `host/` contains the standalone C++ host foundation.
- `docs/architecture.md` describes the major layers.
- `docs/behavior-catalog.md` describes the behavior database format.
- `docs/protocol-v2.md` sketches the binary wire protocol.
- `docs/arduino-api.md` sketches the intended Arduino user experience.
- `catalog/base-behaviors.json` is a small starter behavior catalog.
- `firmware/arduino/libraries/CockpitLink` contains the starter Arduino
  library shell.

## Build

CockpitLink is independently buildable from this folder:

```powershell
cmake -S CockpitLink -B out/build/cockpitlink-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/cockpitlink-Debug
ctest --test-dir out/build/cockpitlink-Debug --output-on-failure
```

The current host probe app lists candidate serial ports:

```powershell
out/build/cockpitlink-Debug/CockpitLinkProbe.exe
```

If a board is flashed with the CockpitLink `ButtonBox` example, the probe will
send a binary `Hello` frame and should receive a `HelloAck` containing the
device name and firmware version.

The current `ButtonBox` example also requests `lights.beacon` for
`LED_BUILTIN`. The probe assigns handle `0`, accepts the subscription, and sends
four fake boolean updates two seconds apart so the LED can prove the behavior
path.

The probe writes raw and decoded serial trace output to:

```text
CockpitLink/logs/CockpitLinkSerial.log
```

## Non-Goals For The First Pass

- Replacing Phoenix.
- Supporting every simulator behavior up front.
- Hiding advanced direct dataref/command access from users who need it.
- Designing the final binary byte layout before proving the behavior model.

## First Vertical Slice

The first prototype should prove:

1. A board asks for behavior IDs.
2. The host resolves those IDs through an X-Plane adapter.
3. The host assigns typed behavior handles.
4. The board can bind a switch and LED to one behavior.
5. The serial log renders binary frames as readable text.
