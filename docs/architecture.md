# CockpitLink Architecture

CockpitLink is organized around four layers.

## Hardware API

The hardware API is what Arduino users see. It should describe devices and
behavior:

```cpp
CockpitLink.switchInput(7).controls("lights.beacon");
CockpitLink.digitalOutput(LED_BUILTIN).follows("lights.beacon");
```

The low-level simulator concepts are still available for advanced users, but
they should not be required for common cockpit hardware.

## Behavior Model

The behavior model defines simulator-neutral concepts:

- ID: `lights.beacon`
- Kind: `toggle`
- Value type: `bool`
- Units: optional
- Direction: readable, writable, or both
- Desired capability versus actual simulator capability
- Recommended update policy

Behavior IDs should be stable. Bindings may change by simulator, aircraft, or
profile.

The host implementation exposes this model through
`cockpitlink::catalog::BehaviorCatalog`. It parses and validates the base JSON
catalog, assigns transport handles from catalog order for the current session,
and resolves typed X-Plane read/write operations, array indices, scaling, and
write strategies. Protocol handles remain transport details and are not stored
in catalog data.

The behavior model must account for simulators having different abilities. A
behavior can be conceptually read/write while a specific simulator binding is
read-only, command-only, unsupported, or emulated through another operation.

## Simulator Adapters

Adapters translate behaviors into simulator-specific operations.

Examples:

- X-Plane dataref read
- X-Plane command trigger
- MSFS simvar read
- MSFS event write
- DCS module export or input binding

Adapters should expose a common host-side interface:

```text
resolve behavior -> binding plan
read behavior value
write behavior value
trigger behavior action
```

The binding plan includes capability status. For example, an X-Plane adapter
may support beacon lights as `read=native` and `write=emulatedByCommand`, while
another simulator may expose the same behavior as `read=native` and
`write=unsupported`.

Adapters must also expose write strategy. A toggle command is not the same as a
direct state write. For switch-style hardware, the host should only use a toggle
command to set state when it can first read the current simulator value and
determine that a toggle is actually needed.

The X-Plane prototype builds its boolean and numeric binding tables from the
runtime catalog. Its post-build step copies the base catalog beside the plugin,
under `catalog/base-behaviors.json`.

## Transport Layer

The transport layer moves typed messages between host and device. It should not
know about X-Plane, MSFS, or DCS.

Initial transports:

- USB serial
- TCP client/server
- UDP discovery plus TCP session

The same message model should run over each transport.

The first host skeleton includes:

- `cockpitlink::transport::IByteTransport`
- Windows serial enumeration
- Windows serial byte transport
- Serial device classification
- `CockpitLinkProbe`, a console app that lists candidate serial devices

### Host Transport Scheduling

The X-Plane adapter uses `cockpitlink::transport::TransportSession` to keep
serial work bounded inside each flight-loop callback. The session:

- retains parser state across partial reads;
- limits read size, read passes, and delivered messages per tick;
- retains partially written frames across ticks;
- performs at most one bounded physical write per tick;
- prioritizes reliable setup/configuration frames;
- coalesces replaceable streaming frames by behavior handle.

The current X-Plane limits are a 256-byte read buffer, four read passes,
16 delivered messages, and 64 written bytes per tick. Windows serial writes
use a 10 ms timeout instead of the earlier 500 ms timeout.

Firmware registration is incremental. After `Hello`, the board emits at most
one behavior request every 25 ms rather than blocking inside the handshake and
bursting the entire behavior list.

## Diagnostic Layer

The wire format may be binary, but the user-facing log must remain readable.

Example decoded trace:

```text
TX hello-ack protocol=2 max-payload=512 profile=matched
RX subscribe behavior=lights.beacon rate=100 bucket=1
TX update behavior=lights.beacon type=bool value=true
RX command behavior=autopilot.ap_disconnect action=trigger count=1
```

Diagnostics are part of the design, not an afterthought. The host should decode
binary frames into readable trace lines and include capability failures in the
same log stream.
