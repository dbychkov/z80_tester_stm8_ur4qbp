<#
        Script: publish-release.ps1
        Author: dbychkov
        Created: 2026-04-26

        Purpose:
            Build Release_CC and Release_CA firmware, create/push a git tag,
            and publish a GitHub release with both .s19 assets.

        Prerequisites:
            - STVD installed (or pass -StvdExePath)
            - git and gh available in PATH
            - Run from inside repository (default ProjectRoot = parent of this script folder)

        Version behavior:
            - If -Version is provided, it is used as release tag version.
            - If -Version is omitted, version is auto-derived from main.h:
              v<FIRMWARE_VERSION_MAJOR>.<FIRMWARE_VERSION_MINOR>.0

        Typical usage:
            .\publish-release.ps1 -Version v1.2.0
            .\publish-release.ps1
            .\publish-release.ps1 -Draft
            .\publish-release.ps1 -SkipBuild -AllowDirty

        Optional flags:
            -Draft        Create a draft GitHub release
            -SkipBuild    Skip STVD build and only publish existing artifacts
            -AllowDirty   Allow uncommitted changes in working tree

        Optional file parameters:
            -ProjectRoot      Project root path (defaults to build\..)
            -ProjectFile      STVD project file (default: z80_tester.stp)
            -MainHeaderFile   Header file with FIRMWARE_VERSION_* defines (default: main.h)
            -ReleaseNotesFile Path to custom release notes for gh release create
            -StvdExePath      Explicit path to STVD.exe

        Change Log:
            - 2026-04-26 dbychkov: Initial script header and release automation.
            - 2026-04-26 dbychkov: Added auto-version from main.h and build folder root handling.
#>

param(
    [ValidatePattern('^v?\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [string]$ProjectRoot = (Join-Path $PSScriptRoot ".."),
    [string]$ProjectFile = "z80_tester.stp",
    [string]$MainHeaderFile = "main.h",
    [string]$StvdExePath,
    [string]$ReleaseNotesFile,

    [switch]$Draft,
    [switch]$SkipBuild,
    [switch]$AllowDirty
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    Write-Host "[STEP] $Message" -ForegroundColor Cyan
}

function Write-Info {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Gray
}

function Resolve-StvdPath {
    param([string]$UserPath)

    if ($UserPath) {
        if (Test-Path $UserPath) {
            return (Resolve-Path $UserPath).Path
        }
        throw "STVD executable not found at: $UserPath"
    }

    $candidates = @(
        "C:\Program Files (x86)\STMicroelectronics\st_toolset\stvd\STVD.exe",
        "C:\Program Files\STMicroelectronics\st_toolset\stvd\STVD.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }

    $fromPath = Get-Command STVD.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    throw "STVD.exe not found. Pass -StvdExePath explicitly or add STVD.exe to PATH."
}

function Invoke-StvdBuild {
    param(
        [string]$StvdExe,
        [string]$ProjectPath,
        [string]$Configuration
    )

    # STVD command-line support varies by version. This uses the common project/config build pattern.
    $args = @(
        "-Project", $ProjectPath,
        "-Configuration", $Configuration,
        "-Build"
    )

    Write-Info "Running: $StvdExe $($args -join ' ')"
    $proc = Start-Process -FilePath $StvdExe -ArgumentList $args -Wait -PassThru -NoNewWindow
    if ($proc.ExitCode -ne 0) {
        throw "STVD build failed for '$Configuration' (exit code $($proc.ExitCode))."
    }
}

function Assert-Tool {
    param([string]$Tool)
    if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        throw "Required tool '$Tool' was not found in PATH."
    }
}

function Assert-File {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "Required file not found: $Path"
    }
}

function Get-VersionFromMainHeader {
    param([string]$HeaderPath)

    $content = Get-Content -Path $HeaderPath -Raw
    $majorMatch = [regex]::Match($content, '(?m)^\s*#define\s+FIRMWARE_VERSION_MAJOR\s+(\d+)\s*$')
    $minorMatch = [regex]::Match($content, '(?m)^\s*#define\s+FIRMWARE_VERSION_MINOR\s+(\d+)\s*$')

    if (-not $majorMatch.Success -or -not $minorMatch.Success) {
        throw "Failed to parse FIRMWARE_VERSION_MAJOR/MINOR from: $HeaderPath"
    }

    $major = $majorMatch.Groups[1].Value
    $minor = $minorMatch.Groups[1].Value
    return "v$major.$minor.0"
}

$mainHeaderPath = Join-Path $ProjectRoot $MainHeaderFile
$resolvedVersion = $Version
if ([string]::IsNullOrWhiteSpace($resolvedVersion)) {
    Assert-File $mainHeaderPath
    $resolvedVersion = Get-VersionFromMainHeader -HeaderPath $mainHeaderPath
    Write-Info "Auto-derived release version from $MainHeaderFile: $resolvedVersion"
}

$normalizedVersion = if ($resolvedVersion.StartsWith("v")) { $resolvedVersion } else { "v$resolvedVersion" }
$projectPath = Join-Path $ProjectRoot $ProjectFile
$ccS19 = Join-Path $ProjectRoot "Release_CC\z80_tester_cc.s19"
$caS19 = Join-Path $ProjectRoot "Release_CA\z80_tester_ca.s19"

Write-Step "Checking required tools and repository state"
Assert-Tool "git"
Assert-Tool "gh"
Assert-File $projectPath

Push-Location $ProjectRoot
try {
    $insideRepo = (git rev-parse --is-inside-work-tree 2>$null)
    if ($LASTEXITCODE -ne 0 -or $insideRepo -ne "true") {
        throw "ProjectRoot is not inside a git repository: $ProjectRoot"
    }

    if (-not $AllowDirty) {
        $dirty = git status --porcelain
        if ($dirty) {
            throw "Working tree is not clean. Commit/stash changes or rerun with -AllowDirty."
        }
    }

    Write-Step "Building Release_CC and Release_CA"
    if (-not $SkipBuild) {
        $stvd = Resolve-StvdPath -UserPath $StvdExePath
        Invoke-StvdBuild -StvdExe $stvd -ProjectPath $projectPath -Configuration "Release_CC"
        Invoke-StvdBuild -StvdExe $stvd -ProjectPath $projectPath -Configuration "Release_CA"
    } else {
        Write-Info "Skipping build step as requested."
    }

    Write-Step "Validating release firmware artifacts"
    Assert-File $ccS19
    Assert-File $caS19

    Write-Step "Creating and pushing tag $normalizedVersion"
    $existingTag = git tag --list $normalizedVersion
    if ($existingTag) {
        throw "Tag already exists: $normalizedVersion"
    }

    git tag -a $normalizedVersion -m "Release $normalizedVersion"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to create tag $normalizedVersion"
    }

    $currentBranch = (git rev-parse --abbrev-ref HEAD).Trim()
    git push origin $currentBranch
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to push branch '$currentBranch'"
    }

    git push origin $normalizedVersion
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to push tag '$normalizedVersion'"
    }

    Write-Step "Publishing GitHub release with both .s19 assets"
    $ghArgs = @("release", "create", $normalizedVersion, $ccS19, $caS19, "--title", $normalizedVersion)

    if ($Draft) {
        $ghArgs += "--draft"
    }

    if ($ReleaseNotesFile) {
        Assert-File $ReleaseNotesFile
        $ghArgs += @("--notes-file", $ReleaseNotesFile)
    } else {
        $ghArgs += @("--generate-notes")
    }

    & gh @ghArgs
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub release creation failed."
    }

    Write-Host "Release published successfully: $normalizedVersion" -ForegroundColor Green
    Write-Host "Assets:" -ForegroundColor Green
    Write-Host " - $ccS19" -ForegroundColor Green
    Write-Host " - $caS19" -ForegroundColor Green
}
catch {
    Write-Error $_
    exit 1
}
finally {
    Pop-Location
}
