# Releasing Firmware to GitHub

This document describes how to create and publish a new GitHub release for this project, including both Release firmware binaries:

- Common cathode firmware: Release_CC/z80_tester_cc.s19
- Common anode firmware: Release_CA/z80_tester_ca.s19

## 1. Prepare Working Tree

1. Ensure all intended code and documentation changes are committed.
2. Ensure the branch is up to date with remote.
3. Verify version information if needed (for example in main.h and README.md).

## 2. Build Release Firmware Variants in STVD

1. Open the workspace/project in ST Visual Develop.
2. Select configuration: Release_CC.
3. Build the project.
4. Confirm generated binary exists:
   - Release_CC/z80_tester_cc.s19
5. Select configuration: Release_CA.
6. Build the project.
7. Confirm generated binary exists:
   - Release_CA/z80_tester_ca.s19

## 3. Sanity Check Artifacts

1. Confirm both .s19 files are from the same source revision.
2. Optionally program each file with STVP and do a quick functional smoke test:
   - z80_tester_cc.s19 on common cathode hardware
   - z80_tester_ca.s19 on common anode hardware

## 4. Create a Version Tag

Use semantic versioning format (example: v1.1.0).

PowerShell example:

git checkout main
git pull
git tag -a v1.1.0 -m "Release v1.1.0"
git push origin main
git push origin v1.1.0

## 5. Publish GitHub Release (Web UI)

1. Open repository on GitHub.
2. Go to Releases -> Draft a new release.
3. Select tag: v1.1.0 (or create new tag from target branch).
4. Release title: v1.1.0
5. Add release notes (summary of changes, fixes, and compatibility notes).
6. Attach both firmware binaries:
   - Release_CC/z80_tester_cc.s19
   - Release_CA/z80_tester_ca.s19
7. Publish release.

## 6. Publish GitHub Release (CLI Alternative)

If GitHub CLI is available:

gh release create v1.1.0 \
  --title "v1.1.0" \
  --notes "See README change log and release notes." \
  Release_CC/z80_tester_cc.s19 \
  Release_CA/z80_tester_ca.s19

## 7. Post-Release Checklist

1. Verify both attached assets are downloadable from release page.
2. Verify asset names clearly indicate display type:
   - z80_tester_cc.s19 = common cathode
   - z80_tester_ca.s19 = common anode
3. Announce release and include flashing note: STVP should program the .s19 files.

## Notes

- This repository also has Debug variants. Only Release_CC and Release_CA binaries should be published as release assets.
- If needed, keep release notes aligned with the Change Log section in README.md.
