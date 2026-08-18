## Summary

<!-- What changes, and which user or maintenance scenario does it address? -->

## Verification

- [ ] `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- [ ] `cmake --build build`
- [ ] `ctest --test-dir build --output-on-failure`
- [ ] Additional focused checks are listed below.

## Documentation and release impact

- [ ] README/user guide/state/changelog are updated if facts or behavior changed.
- [ ] UI changes include a screenshot or recording.
- [ ] Packaging changes include install, upgrade, uninstall, and checksum considerations.
- [ ] No download URL, benchmark, or compatibility claim is introduced without evidence.

## Safety and accessibility

- [ ] The visible status and immediate stop path remain intact.
- [ ] No concealment, process hiding, monitoring bypass, misleading identity, telemetry, or
      unrequested elevation/network access was added.
- [ ] Keyboard access, readable status, and pause reasons were considered.

## Notes

<!-- Call out limitations, unverified checks, or follow-up work. -->
