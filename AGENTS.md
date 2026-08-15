# CockpitLink Agent Instructions

Before changing firmware bindings, simulator mappings, catalogs, or profiles,
read `docs/ai-build-guide.md` and `docs/behavior-catalog.md`.

## Stable product boundaries

- Treat `catalog/base-behaviors.json` as a versioned, read-only product
  contract during normal hardware and aircraft development.
- Put shareable aircraft differences in `profiles/aircraft/`.
- Put private experiments and user calibration in `profiles/local/`. This
  directory is intentionally ignored by Git.
- Do not put simulator names, aircraft names, command IDs, datarefs, SimVars,
  or Input Event names in semantic behavior IDs.
- Do not change firmware merely to accommodate one simulator or aircraft when
  a profile binding can express the difference.

## Required workflow

1. Record the physical device, pins, electrical behavior, and intended cockpit
   meaning.
2. Reuse an existing semantic behavior when its meaning matches.
3. Add or modify the narrowest valid profile layer.
4. Validate JSON and run the catalog, protocol, and transport tests.
5. Build the affected simulator adapter.
6. Perform a live test for direction, range, detents, hold behavior, feedback,
   reconnect behavior, and simulator shutdown.
7. Record evidence using `docs/diagnostic-report.md`.

Never claim an aircraft profile is validated solely because an API call was
accepted. Simulator movement and state feedback must be observed in a live
test. Preserve unrelated user changes and never commit `profiles/local/`, SDK
files, build output, logs, or simulator installations.
