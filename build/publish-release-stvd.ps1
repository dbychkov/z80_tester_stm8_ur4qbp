<#
    Script: publish-release-stvd.ps1
    Author: dbychkov
    Created: 2026-04-26

    Purpose:
      Build Release_CC and Release_CA firmware with ST Visual Develop (STVD),
      create/push a git tag, and publish a GitHub release with both .s19 assets.

    Prerequisites:
      - STVD installed (default: C:\Program Files (x86)\STMicroelectronics\st_toolset\stvd\stvdebug.exe)
      - git and optionally gh available in PATH
      - Run from inside repository (default ProjectRoot = parent of this script folder)

    Version behavior:
      - If -Version is provided, it is used as release tag version.
      - If -Version is omitted, version is auto-derived from main.h:
        v<FIRMWARE_VERSION_MAJOR>.<FIRMWARE_VERSION_MINOR>.0

    Typical usage:
      .\build\publish-release-stvd.ps1
      .\build\publish-release-stvd.ps1 -SkipPublish -AllowDirty
      .\build\publish-release-stvd.ps1 -Version v1.2.0
      .\build\publish-release-stvd.ps1 -AllowDirty -Version v1.2.0
      .\build\publish-release-stvd.ps1 -Preflight -SkipBuild
      .\build\publish-release-stvd.ps1 -SkipPublish

    Optional flags:
      -Draft        Create a draft GitHub release
      -SkipBuild    Skip firmware build steps
      -SkipPublish  Skip GitHub release publishing step
      -Preflight    Validate prerequisites and inputs, then exit
      -AllowDirty   Allow uncommitted changes in working tree

        Optional file parameters:
            -ProjectRoot      Project root path (defaults to build\..)
            -ProjectFile      STVD project file (default: z80_tester.stp)
            -MainHeaderFile   Header file with FIRMWARE_VERSION_* defines (default: main.h)
            -ReleaseNotesFile Path to custom release notes for gh release create
            -StvdPath         Full path to stvdebug.exe
            -CosmicBinPath    Explicit folder containing cxstm8.exe/clnk.exe/chex.exe

        Notes:
            STVD's GUI project file (.stp) is treated as the source of truth for build
            configurations and output names. The build itself is executed headlessly from
            the command lines encoded in the STVD project settings.
#>

param(
    [ValidatePattern('^v?\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [string]$ProjectRoot = (Join-Path $PSScriptRoot ".."),
    [string]$ProjectFile = "z80_tester.stp",
    [string]$MainHeaderFile = "main.h",
    [string]$ReleaseNotesFile,
    [string]$StvdPath = "C:\Program Files (x86)\STMicroelectronics\st_toolset\stvd\stvdebug.exe",
    [string]$CosmicBinPath,

    [switch]$Draft,
    [switch]$SkipBuild,
    [switch]$SkipPublish,
    [switch]$Preflight,
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

function Assert-Tool {
    param([string]$Tool)
    if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        throw "Required tool '$Tool' was not found in PATH."
    }
}

function Test-ToolExists {
    param([string]$Tool)
    return [bool](Get-Command $Tool -ErrorAction SilentlyContinue)
}

function Assert-File {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "Required file not found: $Path"
    }
}

function Resolve-CosmicTool {
    param(
        [string]$ExeName,
        [string]$BinPath
    )

    if ($BinPath) {
        $candidate = Join-Path $BinPath $ExeName
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
        throw "COSMIC tool not found at: $candidate"
    }

    $fromPath = Get-Command $ExeName -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $candidates = @(
        ("C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8\{0}" -f $ExeName),
        ("C:\Program Files\COSMIC\FSE_Compilers\CXSTM8\{0}" -f $ExeName)
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }

    throw "Required COSMIC tool '$ExeName' not found. Add it to PATH or pass -CosmicBinPath."
}

function Invoke-External {
    param(
        [string]$ExePath,
        [string[]]$CmdLineArgs,
        [string]$ErrorContext
    )

    Write-Info "Running: $ExePath $($CmdLineArgs -join ' ')"
    & $ExePath @CmdLineArgs
    if ($LASTEXITCODE -ne 0) {
        throw "$ErrorContext (exit code $LASTEXITCODE)."
    }
}

function Get-FirmwareVersionsFromMainHeader {
    param([string]$HeaderPath)

    $content = Get-Content -Path $HeaderPath -Raw
    $majorMatch = [regex]::Match($content, '(?m)^\s*#define\s+FIRMWARE_VERSION_MAJOR\s+(\d+)\s*$')
    $minorMatch = [regex]::Match($content, '(?m)^\s*#define\s+FIRMWARE_VERSION_MINOR\s+(\d+)\s*$')

    if (-not $majorMatch.Success -or -not $minorMatch.Success) {
        throw "Failed to parse FIRMWARE_VERSION_MAJOR/MINOR from: $HeaderPath"
    }

    $major = $majorMatch.Groups[1].Value
    $minor = $minorMatch.Groups[1].Value
    return @{
        Major          = $major
        Minor          = $minor
        CodeVersion    = "v$major.$minor"
        ReleaseVersion = "v$major.$minor.0"
    }
}

function Get-IniSections {
    param([string]$FilePath)

    $sections = @{}
    $currentSection = $null

    foreach ($line in Get-Content -Path $FilePath) {
        if ($line -match '^\[(.+)\]$') {
            $currentSection = $Matches[1]
            $sections[$currentSection] = @{}
            continue
        }

        if (-not $currentSection) {
            continue
        }

        if ([string]::IsNullOrWhiteSpace($line) -or $line.TrimStart().StartsWith(';')) {
            continue
        }

        $delimiterIndex = $line.IndexOf('=')
        if ($delimiterIndex -lt 0) {
            continue
        }

        $key = $line.Substring(0, $delimiterIndex)
        $value = $line.Substring($delimiterIndex + 1)
        $sections[$currentSection][$key] = $value
    }

    return $sections
}

function Get-StvdProjectDefinition {
    param(
        [string]$ProjectPath
    )

    $sections = Get-IniSections -FilePath $ProjectPath
    $configDefinitions = @{}

    foreach ($configSectionName in $sections.Keys | Where-Object { $_ -match '^Config\.\d+$' }) {
        $section = $sections[$configSectionName]
        $configName = $section['ConfigName']
        if ([string]::IsNullOrWhiteSpace($configName)) {
            continue
        }

        $configIndex = [int]($configSectionName.Split('.')[1])
        $compileSection = $sections["Root.Config.$configIndex.Settings.3"]
        $linkSection = $sections["Root.Config.$configIndex.Settings.6"]
        $postBuildSection = $sections["Root.Config.$configIndex.Settings.7"]
        $pathsSection = $sections["Root.Config.$configIndex.Settings.1"]

        $configDefinitions[$configName] = @{
            Index            = $configIndex
            OutputFolder     = $section['OutputFolder']
            TargetFileName   = $section['Target']
            CompileCommand   = $compileSection['String.3.0']
            LinkCommand      = $linkSection['String.3.0']
            PostBuildCommand = $postBuildSection['String.3.0']
            IncludePaths     = $pathsSection['String.103.0']
        }
    }

    $sourceFiles = @()
    $currentSection = $sections['Root.Source Files']['Child']
    while (-not [string]::IsNullOrWhiteSpace($currentSection)) {
        $fileSection = $sections[$currentSection]
        if (-not $fileSection) {
            break
        }

        if ($fileSection['ElemType'] -eq 'File') {
            $sourceFiles += $fileSection['PathName']
        }

        $currentSection = $fileSection['Next']
    }

    return @{
        Configs     = $configDefinitions
        SourceFiles = $sourceFiles
    }
}

function Get-IncludeArguments {
    param([string]$IncludePaths)

    $args = @()
    foreach ($pathEntry in ($IncludePaths -split ';')) {
        if ([string]::IsNullOrWhiteSpace($pathEntry)) {
            continue
        }

        $normalized = $pathEntry.Trim()
        if ($normalized -eq '.') {
            continue
        }

        $normalized = $normalized.TrimEnd('\')
        if ($normalized -eq '.\') {
            continue
        }

        $args += "-i$normalized"
    }

    return $args
}

function Invoke-StvdProjectBuildConfiguration {
    param(
        [string]$ProjectRootPath,
        [hashtable]$ProjectDefinition,
        [string]$ConfigName,
        [string]$CxPath,
        [string]$ClnkPath,
        [string]$ChexPath
    )

    $config = $ProjectDefinition.Configs[$ConfigName]
    if (-not $config) {
        throw "STVD config '$ConfigName' was not found in project definition."
    }

    $targetBaseName = [System.IO.Path]::GetFileNameWithoutExtension($config.TargetFileName)
    $outputPath = Join-Path $ProjectRootPath $config.OutputFolder
    if (-not (Test-Path $outputPath)) {
        New-Item -ItemType Directory -Path $outputPath | Out-Null
    }

    $outputPathAbs = (Resolve-Path $outputPath).Path
    $includeArgs = Get-IncludeArguments -IncludePaths $config.IncludePaths

    $displayCommonAnodeMatch = [regex]::Match($config.CompileCommand, '-dDISPLAY_COMMON_ANODE=(\d+)')
    if (-not $displayCommonAnodeMatch.Success) {
        throw "Failed to parse DISPLAY_COMMON_ANODE value from STVD compile command for '$ConfigName'."
    }

    $displayCommonAnode = $displayCommonAnodeMatch.Groups[1].Value

    foreach ($src in $ProjectDefinition.SourceFiles) {
        $srcPath = Join-Path $ProjectRootPath $src
        Assert-File $srcPath

        $compileArgs = @(
            '+mods0',
            '+compact',
            "-dDISPLAY_COMMON_ANODE=$displayCommonAnode"
        ) + $includeArgs + @(
            "-cl$outputPathAbs\\",
            "-co$outputPathAbs\\",
            $srcPath
        )

        Invoke-External -ExePath $CxPath -CmdLineArgs $compileArgs -ErrorContext "Compile failed for '$src' in '$ConfigName'"
    }

    $lkfPath = Join-Path $outputPathAbs ("{0}.lkf" -f $targetBaseName)
    $autoGeneratedMarker = '# Auto-generated by publish-release-stvd.ps1'
    $templateContent = @"
# LINK COMMAND FILE AUTOMATICALLY GENERATED BY publish-release-stvd.ps1 (STVD project-driven)
# Segment Code,Constants:
+seg .const -b 0x8080 -m 0x1f80 -n .const -it
+seg .text -a .const -n .text
# Segment Eeprom:
+seg .eeprom -b 0x4000 -m 0x280 -n .eeprom
# Segment Zero Page:
+seg .bsct -b 0x0 -m 0x100 -n .bsct
+seg .ubsct -a .bsct -n .ubsct
+seg .bit -a .ubsct -n .bit -id
+seg .share -a .bit -n .share -is
# Segment Ram:
+seg .data -b 0x100 -m 0x100 -n .data
+seg .bss -a .data -n .bss

# Startup file
crtsi0.sm8

# Object files
$(($ProjectDefinition.SourceFiles | Where-Object {
    [System.IO.Path]::GetFileName($_) -ne 'stm8_interrupt_vector.c'
} | ForEach-Object {
    '{0}\{1}.o' -f $config.OutputFolder, [System.IO.Path]::GetFileNameWithoutExtension($_)
}) -join "`r`n")

# Library list
libis0.sm8
libm0.sm8

# Defines
+def __endzp=@.ubsct
+def __memory=@.bss
+def __startmem=@.bss
+def __endmem=0x1ff
+def __stack=0x3ff

# Interrupt vectors
+seg .const -b 0x8000 -k
$($config.OutputFolder)\stm8_interrupt_vector.o

$autoGeneratedMarker
"@
    Set-Content -Path $lkfPath -Value $templateContent -Encoding ASCII

    $sm8Path = Join-Path $outputPathAbs ("{0}.sm8" -f $targetBaseName)
    $s19Path = Join-Path $outputPathAbs ("{0}.s19" -f $targetBaseName)
    $mapPath = Join-Path $outputPathAbs ("{0}.map" -f $targetBaseName)

    Push-Location $ProjectRootPath
    try {
        Invoke-External -ExePath $ClnkPath -CmdLineArgs @("-m$mapPath", "-o$sm8Path", $lkfPath) -ErrorContext "Link failed for '$ConfigName'"
    }
    finally {
        Pop-Location
    }

    Invoke-External -ExePath $ChexPath -CmdLineArgs @("-o$s19Path", $sm8Path) -ErrorContext "S19 generation failed for '$ConfigName'"
}

$mainHeaderPath = Join-Path $ProjectRoot $MainHeaderFile
Assert-File $mainHeaderPath
$firmwareVersions = Get-FirmwareVersionsFromMainHeader -HeaderPath $mainHeaderPath
$codeVersion = $firmwareVersions.CodeVersion

$resolvedVersion = $Version
if ([string]::IsNullOrWhiteSpace($resolvedVersion)) {
    $resolvedVersion = $firmwareVersions.ReleaseVersion
    Write-Info "Auto-derived release version from ${MainHeaderFile}: $resolvedVersion"
}

$normalizedVersion = if ($resolvedVersion.StartsWith("v")) { $resolvedVersion } else { "v$resolvedVersion" }
$projectPath = Join-Path $ProjectRoot $ProjectFile
$ccS19 = Join-Path $ProjectRoot "Release_CC\z80_tester_cc.s19"
$caS19 = Join-Path $ProjectRoot "Release_CA\z80_tester_ca.s19"

Write-Step "Checking required tools and repository state"
Assert-Tool "git"
Assert-File $projectPath
Assert-File $StvdPath

$cxTool = $null
$clnkTool = $null
$chexTool = $null
$projectDefinition = $null

$hasGh = Test-ToolExists "gh"
if (-not $hasGh -and -not $SkipPublish) {
    Write-Warning "GitHub CLI (gh) was not found in PATH. Switching to -SkipPublish mode."
    Write-Warning "Install gh from https://cli.github.com/ to enable automated release publishing."
    Write-Host "Install hint: winget install --id GitHub.cli -e" -ForegroundColor Yellow
    $SkipPublish = $true
}

Write-Info "Using STVD project file: $projectPath"
Write-Info "Using STVD installation: $StvdPath"

if (-not $SkipBuild) {
    $cxTool = Resolve-CosmicTool -ExeName 'cxstm8.exe' -BinPath $CosmicBinPath
    $clnkTool = Resolve-CosmicTool -ExeName 'clnk.exe' -BinPath $CosmicBinPath
    $chexTool = Resolve-CosmicTool -ExeName 'chex.exe' -BinPath $CosmicBinPath
    $projectDefinition = Get-StvdProjectDefinition -ProjectPath $projectPath

    Write-Info "Using STM8 Cosmic tools from STVD project settings:"
    Write-Info " - cxstm8: $cxTool"
    Write-Info " - clnk:   $clnkTool"
    Write-Info " - chex:   $chexTool"
}

if ($Preflight) {
    Write-Host "Preflight checks passed." -ForegroundColor Green
    exit 0
}

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

    Write-Step "Building Release_CC and Release_CA from STVD project settings"
    if (-not $SkipBuild) {
        Invoke-StvdProjectBuildConfiguration -ProjectRootPath $ProjectRoot -ProjectDefinition $projectDefinition -ConfigName 'Release_CC' -CxPath $cxTool -ClnkPath $clnkTool -ChexPath $chexTool
        Invoke-StvdProjectBuildConfiguration -ProjectRootPath $ProjectRoot -ProjectDefinition $projectDefinition -ConfigName 'Release_CA' -CxPath $cxTool -ClnkPath $clnkTool -ChexPath $chexTool
    } else {
        Write-Info "Skipping build step as requested."
    }

    Write-Step "Validating release firmware artifacts"
    Assert-File $ccS19
    Assert-File $caS19

    if (-not $SkipPublish) {
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

        $assetNotes = @"
Firmware code version: $codeVersion

Release assets:
- z80_tester_cc.s19: Common cathode firmware (DISPLAY_COMMON_ANODE=0).
- z80_tester_ca.s19: Common anode firmware (DISPLAY_COMMON_ANODE=1).

The .s19 files are the firmware images that should be programmed into the STM8 MCU using ST Visual Programmer (STVP).
"@

        if ($Draft) {
            $ghArgs += "--draft"
        }

        if ($ReleaseNotesFile) {
            Assert-File $ReleaseNotesFile
            $customNotes = Get-Content -Path $ReleaseNotesFile -Raw
            if ([string]::IsNullOrWhiteSpace($customNotes)) {
                $ghArgs += @("--notes", $assetNotes)
            } else {
                $ghArgs += @("--notes", ($assetNotes + "`r`n" + $customNotes))
            }
        } else {
            $ghArgs += @("--notes", $assetNotes, "--generate-notes")
        }

        & gh @ghArgs
        if ($LASTEXITCODE -ne 0) {
            throw "GitHub release creation failed."
        }
    } else {
        Write-Warning "Skipping git tag creation, push, and GitHub release publish."
        Write-Host "You can publish manually later and attach:" -ForegroundColor Yellow
        Write-Host " - $ccS19" -ForegroundColor Yellow
        Write-Host " - $caS19" -ForegroundColor Yellow
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
