# CockpitLink Diagnostic Report

Use this report when asking an AI assistant or maintainer to create a profile,
diagnose a control, or reproduce a transport problem. Replace bracketed text;
do not include access tokens, account data, or unrelated serial devices.

## Environment

```text
CockpitLink commit/version: [git commit or release]
Operating system: [Windows version]
Simulator: [X-Plane/MSFS and version]
Aircraft title: [exact title reported by the host]
Aircraft package/version: [stock/add-on/livery if known]
Host executable: [X-Plane plugin or CockpitLinkMSFS]
Loaded catalog layers, in order:
  1. [base]
  2. [simulator profile, if any]
  3. [aircraft profile, if any]
  4. [device profile, if any]
  5. [local user profile(s), if any]
```

## Device

```text
Board: [for example Arduino Mega 2560]
Firmware example/sketch: [name and commit]
Firmware identity reported by host: [exact line]
Serial port: [for example COM7]
Baud rate: [normally 115200]
Power arrangement: [USB/external, common ground]
Control: [pot/switch/button/encoder/display]
Pins: [all relevant pins]
Electrical behavior: [INPUT_PULLUP, active-low, pot endpoints, etc.]
Semantic behavior ID: [for example engine.1.throttle]
```

## Observed behavior

```text
Connection succeeds: [yes/no]
Behavior resolves: [yes/no]
Physical input reaches host: [yes/no/unknown]
Simulator responds: [yes/no/intermittent]
State feedback returns: [yes/no/not applicable]
Direction: [correct/reversed]
Observed physical range: [raw minimum/center/maximum if available]
Observed simulator range: [minimum/detents/maximum]
Single press/click: [result]
Held input: [result and approximate rate]
Fast encoder movement: [result]
After reconnect: [result]
After aircraft reload: [result]
```

Use pilot language if it is the clearest description, but pair it with an
observable result. For example: `power lever stops at flight idle; it never
enters ground fine or reverse` is more useful than `lower range negative`.

## Relevant diagnostics

Include only the lines surrounding the failure:

```text
Catalog layers:
Catalog override:
Connected to:
identified firmware:
device connected:
unsupported behavior:
Resolved Input Event:
SimConnect exception:
transport/parser/reconnect messages:
```

For MSFS aircraft-specific controls, run `CockpitLinkMSFSProbe.exe` while
sitting in the loaded aircraft and include the exact aircraft title and any
relevant Input Events. For X-Plane, include the dataref or command name and
whether it is readable, writable, or command-only.

## Expected result

State the intended cockpit behavior in simulator-neutral terms:

```text
When [physical action], [semantic cockpit result] should occur.
Direction/range/detents: [details]
Feedback/display requirement: [details]
Acceptable update or repeat rate: [details]
Aircraft-specific exception permitted: [yes/no and why]
```

## Reproduction steps

1. [Start simulator and load aircraft.]
2. [Start CockpitLink with listed layers.]
3. [Perform one physical action.]
4. [Observe a named cockpit control or value.]
5. [Repeat after reconnect or reload if relevant.]

## Acceptance record

After a fix, append:

```text
Test date:
Tester:
Commit/profile revision:
Aircraft and route or ground-test state:
Controls tested:
Pass/fail observations:
Known limitations intentionally deferred:
```
