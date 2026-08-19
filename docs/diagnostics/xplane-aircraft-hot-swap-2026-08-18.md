# X-Plane Aircraft Hot-Swap Acceptance — 2026-08-18

## Environment

```text
CockpitLink base commit: 091398a plus uncommitted hot-swap implementation
Operating system: Windows
Simulator: X-Plane 12
Host executable: CockpitLink X-Plane plugin
Catalog layers: catalog/base-behaviors.json
Aircraft observed:
  - Robinson R22 Beta II.acf
  - RV-10.acf
  - N844X.acf (Lancair Evolution)
```

## Device

```text
Firmware identity: Concept Button Box firmware Aug 14 2026 17:34:26
Serial port: COM7
Baud rate: 115200
Device connection was retained across aircraft changes.
```

## Observed behavior

```text
Connection succeeds: yes
Behavior resolves: yes
Physical input reaches host: yes
Simulator responds: yes, visually confirmed by tester
After aircraft reload: pass
After a second aircraft reload: pass
```

On each user-aircraft unload, CockpitLink logged that bindings were paused. It
then identified the new `.acf` filename and full path, rebuilt the layered
catalog, and atomically activated the replacement binding table. The Arduino
did not reconnect or repeat firmware registration. Existing semantic handles
continued producing roll, pitch, throttle, propeller, and mixture writes after
the swap. No catalog, missing-dataref, missing-command, or binding activation
errors were present in the inspected log.

Relevant lifecycle evidence:

```text
CockpitLink: aircraft unloading; bindings paused.
CockpitLink: activated aircraft configuration for RV-10.acf [...RV-10.acf].
CockpitLink: aircraft unloading; bindings paused.
CockpitLink: activated aircraft configuration for N844X.acf [...N844X.acf].
```

## Reproduction steps

1. Start X-Plane with the Concept Button Box attached.
2. Load the Robinson R22 and allow CockpitLink to connect on COM7.
3. Change to the RV-10 without restarting X-Plane or the Arduino.
4. Move the physical flight and engine controls and observe the simulator.
5. Change again to the Lancair Evolution and repeat the control check.
6. Inspect `Log.txt` for pause, activation, continued handle traffic, and
   errors.

## Acceptance record

```text
Test date: 2026-08-18
Tester: Michael-CW
Commit/profile revision: 091398a plus hot-swap working tree
Aircraft and state: ground tests across R22, RV-10, and Lancair Evolution
Controls tested: primary axes and engine-control axes
Pass/fail observations: pass; simulator controls visibly responded after swaps
Known limitations intentionally deferred:
  - No custom X-Plane aircraft profile was available to verify a non-base
    binding override during this test.
  - MSFS automatic lifecycle-driven profile selection remains separate work.
```
