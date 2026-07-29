# CockpitLink Protocol V2

The successor protocol should be binary-framed and transport-neutral. The serial
logger decodes frames into readable text.

## Frame Shape

```text
magic | version | type | flags | sequence | payloadLength | payload | checksum
```

Suggested field sizes:

```text
magic         2 bytes  "CL"
version       1 byte
type          1 byte
flags         1 byte
sequence      2 bytes
payloadLength 2 bytes
payload       N bytes
checksum      4 bytes CRC32
```

The first host implementation has an encoder/parser for this shape and tests
partial reads, CRC rejection, leading noise, and embedded `0x00` bytes in the
payload.

## First Implemented Shell

The first working shell implements:

- Host `Hello` frame.
- Firmware `HelloAck` response.
- `Hello` and `HelloAck` payloads as:
  `name\0version\0maxPayload:uint16 protocolMin:uint8 protocolMax:uint8 capabilityFlags:uint16`.
- Host probe that enumerates serial ports, opens candidates, sends `Hello`, and
  reports responding CockpitLink devices.
- Firmware behavior requests from hardware helpers.
- Host behavior assignment for `lights.beacon`.
- Firmware subscription after assignment.
- Host fake bool `ValueUpdate` frames for `lights.beacon`.

## Versioning

The frame header includes one protocol version byte. The current version is `2`.

Handshake payloads also include a supported protocol range:

```text
protocolMin:uint8
protocolMax:uint8
```

If the host and device do not share a supported version, the receiver should
answer with `UnsupportedVersion` when possible and then close or ignore the
session.

## Capabilities

Handshake payloads include `capabilityFlags:uint16`.

Initial flags:

- `serial`: USB/serial transport.
- `tcp`: TCP transport.
- `udpDiscovery`: UDP discovery support.
- `behaviorIds`: device can request simulator-neutral behavior IDs.
- `binaryValues`: device can handle explicit-length binary values.
- `decodedDiagnostics`: device/host supports decoded diagnostic traces.
- `ackRequested`: sender may request acknowledgements.

Capability negotiation is separate from behavior availability. A board may
support behavior IDs while a simulator adapter reports that a specific behavior
is read-only or unsupported.

## Message Families

- Hello and capability negotiation.
- Profile acceptance and handle assignment.
- Behavior registration.
- Subscription requests.
- Typed value updates.
- Command actions.
- Flow control.
- Diagnostics.

## Typed Values

Every value update carries a type and explicit length.

```text
handle:uint16
valueType:uint8
element:int16
valueLength:uint16
valueBytes:N
```

This keeps `0x00` safe inside `string` and `data` payloads.

## Profile Shortcut

If the host has a matching behavior profile:

```text
Host -> Device: profile accepted, dataref/behavior handles are valid
Device -> Host: subscribe to known handles
```

If no matching profile exists:

```text
Host -> Device: send behavior requests
Device -> Host: behavior ID list
Host -> Device: assigned handles
```

## Flow Control

The host should advertise:

- Maximum payload size.
- Maximum bytes per tick.
- Maximum frames per tick.
- Whether acknowledgements are required.

The device should advertise:

- Receive buffer size.
- Transmit buffer size.
- Supported transports.
- Supported protocol versions.

Payload size must be negotiated. Small boards can advertise smaller receive
buffers without forcing the whole protocol to shrink.

## Failure Behavior

Failure frames should be explicit. Initial failure codes:

- `UnsupportedProtocolVersion`
- `BehaviorUnknown`
- `CapabilityUnavailable`
- `PayloadTooLarge`
- `Unauthorized`

Devices should continue operating with partial capability when possible. For
example, if `lights.beacon` is readable but not writable in a simulator, the
host should still allow `follows("lights.beacon")` outputs while rejecting or
reporting switch writes clearly.

## Security For IP Transports

Serial can begin permissive. IP transports should not. Before TCP or UDP control
is enabled, the design should include at least local-network binding choices and
a lightweight pairing or shared-token mechanism so another machine cannot issue
simulator commands accidentally.

## Decoded Log Examples

```text
RX hello device="737 MCP" firmware="2026.07.20" protocol=2 max-rx=512
TX profile accepted behaviors=42 commands=18
RX subscribe handle=12 type=bool rate=100 bucket=1
TX update handle=12 behavior=lights.beacon value=false
RX command handle=4 behavior=autopilot.ap_disconnect action=trigger count=1
```
