# Behavior Schema

This document describes the catalog shape that CockpitLink host adapters should
accept. The schema is intentionally behavior-centered: firmware asks for stable
cockpit behavior IDs, and host adapters translate those behaviors to simulator
details.

## Catalog

Required top-level fields:

- `catalogVersion`: integer schema/catalog version.
- `name`: human-readable catalog name.
- `behaviors`: array of behavior entries.

## Behavior ID

Behavior IDs are stable, lowercase, dot-separated names:

```text
system.component.property
```

Examples:

```text
lights.beacon
gear.handle
autopilot.heading_bug
engine.1.throttle
```

Rules:

- Use lowercase letters, numbers, underscores, and dots.
- Use named expanded entries for arrays, such as `engine.1.throttle`.
- Do not expose simulator array syntax like `engine.throttle[0]`.
- Do not include simulator names, aircraft names, dataref paths, or command
  names in the behavior ID.

## Behavior Entry

Required fields:

- `id`: stable behavior ID.
- `label`: human-readable label.
- `kind`: one of `toggle`, `momentary`, `axis`, `display`, `enum`, `data`.
- `valueType`: one of `bool`, `int`, `float`, `string`, `data`, `enum`.
- `desiredCapability`: requested direction model.
- `defaultUpdate`: default update moderation.
- `bindings`: simulator-specific mapping information.

## Direction Model

`desiredCapability` must contain:

```json
{
  "read": true,
  "write": true,
  "command": false
}
```

The direction keys mean:

- `read`: observe the behavior state.
- `write`: set the behavior to a specific value.
- `command`: trigger an action related to the behavior.

## Binding Capabilities

Simulator bindings should describe what is actually possible with the same
direction keys. Valid capability words are:

- `native`
- `unsupported`
- `emulatedByCommand`
- `emulatedByReadWrite`
- `readOnly`
- `writeOnly`

## Numeric Values

Canonical numeric behaviors should use user-facing units. For example,
`engine.1.throttle` is a percent value from `0` to `100`; an X-Plane adapter may
convert that to a `0.0` to `1.0` dataref value.

Bindings may include `scale` objects to convert between canonical values and
simulator values.

## Catalog Layers

Catalogs merge in this order:

1. Built-in base catalog.
2. Simulator-specific catalog.
3. Aircraft family catalog.
4. Aircraft/version override.
5. User local override.

Later layers may override simulator bindings while preserving behavior IDs.
