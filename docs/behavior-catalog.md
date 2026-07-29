# Behavior Catalog

The behavior catalog maps cockpit intent to simulator-specific bindings.

## Behavior Entry

```json
{
  "id": "lights.beacon",
  "label": "Beacon lights",
  "kind": "toggle",
  "valueType": "bool",
  "desiredCapability": {
    "read": true,
    "write": true,
    "command": true
  },
  "defaultUpdate": {
    "rateMs": 100,
    "bucket": 1
  },
  "bindings": {
    "xplane": {
      "default": {
        "capability": {
          "read": "native",
          "write": "emulatedByCommand",
          "command": "native"
        },
        "read": {
          "dataref": "sim/cockpit/electrical/beacon_lights_on",
          "type": "int"
        },
        "write": {
          "strategy": "setViaToggleWhenKnown",
          "requiresRead": true,
          "toggleCommand": "sim/lights/beacon_lights_toggle",
          "trueValue": 1,
          "falseValue": 0
        }
      }
    }
  }
}
```

## Capability Model

The canonical behavior describes what a cockpit builder would ideally like to
do. Each simulator binding then describes what is actually possible.

Desired capabilities:

- `read`: the host can observe the behavior state.
- `write`: the host can set the behavior to a specific value.
- `command`: the host can trigger an action related to the behavior.

Binding capability values:

- `native`: the simulator supports this directly.
- `emulatedByCommand`: the host can approximate a write by issuing a command,
  usually only when the current state is known.
- `emulatedByReadWrite`: the host can approximate a command by reading and
  writing values.
- `readOnly`: the simulator can report state but cannot change it.
- `writeOnly`: the simulator can change state but cannot report it reliably.
- `unsupported`: this capability is not available for this simulator or
  aircraft.

Example:

```json
{
  "bindings": {
    "xplane": {
      "default": {
        "capability": {
          "read": "native",
          "write": "emulatedByCommand",
          "command": "native"
        }
      }
    },
    "msfs": {
      "default": {
        "capability": {
          "read": "native",
          "write": "unsupported",
          "command": "native"
        }
      }
    }
  }
}
```

When a device requests an unavailable operation, the host should answer with a
clear capability result rather than silently ignoring it.

## Write Strategies

Write capability must describe how the simulator is changed. A command is not
always equivalent to setting a state.

Initial write strategies:

- `directSet`: write the desired value directly to a simulator variable.
- `setViaToggleWhenKnown`: read the current state, compare it with the desired
  value, and only trigger the toggle command if the state differs.
- `commandOnOff`: use separate explicit on/off commands.
- `momentaryCommand`: trigger an action without implying persistent state.
- `unsupported`: no write path exists for this simulator or aircraft.

For physical switches, `setViaToggleWhenKnown` is only safe when the current
simulator state is known. If the host cannot read the current state, it should
report a capability failure instead of blindly toggling.

Example direct set:

```json
"write": {
  "strategy": "directSet",
  "dataref": "sim/cockpit/switches/gear_handle_status",
  "type": "int",
  "trueValue": 1,
  "falseValue": 0
}
```

Example state-aware toggle:

```json
"write": {
  "strategy": "setViaToggleWhenKnown",
  "requiresRead": true,
  "toggleCommand": "sim/lights/beacon_lights_toggle",
  "trueValue": 1,
  "falseValue": 0
}
```

## Catalog Layers

Catalogs should merge in this order:

1. Built-in base catalog.
2. Simulator-specific catalog.
3. Aircraft family catalog.
4. Aircraft/version override.
5. User local override.

Later entries may override simulator bindings while preserving the same behavior
ID.

## Behavior ID Rules

Behavior IDs should be stable, lowercase, and dot-separated:

```text
lights.beacon
autopilot.heading_bug
radios.com1.active_frequency
gear.handle
```

Avoid simulator names, aircraft names, dataref paths, or command names in the ID.
Those belong in bindings and overrides. Once a behavior ID is published, prefer
deprecation plus replacement over changing its meaning.

## Behavior Kinds

Initial kinds:

- `toggle`: boolean state with optional toggle/set commands.
- `momentary`: command-like behavior with start/end/trigger.
- `axis`: numeric value with range and units.
- `display`: read-only formatted value.
- `enum`: finite set of named states.
- `data`: explicit byte payload for special cases.

## Value Types

Initial value types:

- `bool`
- `int`
- `float`
- `string`
- `data`
- `enum`

Data values must always carry an explicit byte length. They must never be
treated as null-terminated strings.

## Units And Scaling

Numeric behaviors should declare units when the value is not dimensionless.

Preferred unit examples:

- `feet`
- `knots`
- `degrees`
- `percent`
- `hertz`
- `megahertz`

Bindings may include simulator-specific conversion, but the canonical behavior
should present one stable unit to firmware and users.

## Open Questions

- Should behavior IDs be fully community-owned, or should the core project
  reserve top-level namespaces?
- Should aircraft overrides be selected manually, detected automatically, or
  both?
- Should writable toggles prefer direct writable values when available, or
  commands for simulator compatibility?
- Should devices declare whether they require exact writes, or whether
  emulated/toggle-based writes are acceptable?
