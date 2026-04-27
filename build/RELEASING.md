# Releasing Firmware to GitHub

This document describes how to create and publish a new GitHub release for this project, including both Release firmware binaries:

- Common cathode firmware: Release_CC/z80_tester_cc.s19
- Common anode firmware: Release_CA/z80_tester_ca.s19

## Quick Start: Automated Release (Recommended)

The project includes an automated build and release script that handles building both firmware variants, creating a git tag, and publishing to GitHub. This is the **recommended approach**.

### Prerequisites

- COSMIC STM8 toolchain installed (standard: `C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8\`)
- git CLI in PATH
- gh CLI in PATH (for GitHub release; optional if using `-SkipPublish`)
- Working directory must be clean (no uncommitted changes) unless `-AllowDirty` is used

### One-Command Release

From the project root, run:

```powershell
.\build\publish-release-stp.ps1
```

This single command will:
1. ? Validate environment and prerequisites
2. ? Parse the STVD project file to extract build settings
3. ? Compile Release_CC and Release_CA firmware variants
4. ? Generate Release_CC/z80_tester_cc.s19 and Release_CA/z80_tester_ca.s19
5. ? Create a git annotated tag (v<MAJOR>.<MINOR>.0 from main.h)
6. ? Push tag and branch to remote
7. ? Create GitHub release with both .s19 assets attached

### Common Usage Scenarios

**Build only (no git/GitHub operations):**
```powershell
.\build\publish-release-stp.ps1 -SkipPublish
```

**Build with uncommitted changes allowed:**
```powershell
.\build\publish-release-stp.ps1 -AllowDirty
```

**Use custom version instead of auto-deriving from main.h:**
```powershell
.\build\publish-release-stp.ps1 -Version v2.1.5
```

**Validate environment without building:**
```powershell
.\build\publish-release-stp.ps1 -Preflight
```

**Publish existing artifacts (skip build):**
```powershell
.\build\publish-release-stp.ps1 -SkipBuild
```

### Build Script Features

**publish-release-stp.ps1 automatically:**
- Reads z80_tester.stp project file to extract source files and compiler flags
- Detects new .c files added to the project (no script editing needed)
- Picks up compiler flag changes made in STVD (dynamic flag extraction)
- Generates both Release_CC and Release_CA variants with correct `-dDISPLAY_COMMON_ANODE` flags
- Creates version tags in semantic format (v#.#.#)
- Attaches firmware binaries to GitHub release with descriptive naming

### For More Details

See [build/README.md](../README.md#building-and-publishing-releases) for comprehensive documentation of script parameters and troubleshooting.

---

## Alternative: Manual Release Process

If you cannot use the automated script (e.g., COSMIC toolchain not available), follow this manual process.

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

## Which Release Method Should I Use?

| Method | When to Use | Pros | Cons |
|--------|-----------|------|------|
| **Automated Script** (`publish-release-stp.ps1`) | Default for all releases | One command; automatic; less error-prone; adapts to project changes | Requires COSMIC toolchain; requires git/gh CLI |
| **Manual Process** | COSMIC toolchain not available; special CI/CD requirements | Complete control over each step; minimal dependencies | Tedious; error-prone; requires remembering multiple commands |

**Recommendation:** Use the automated script (`publish-release-stp.ps1`) whenever possible. It's faster, less error-prone, and automatically handles version tagging and GitHub asset uploads.
