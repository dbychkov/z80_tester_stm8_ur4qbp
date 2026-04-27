# Z80_Tester_STM8_UR4QBP

STM8S103F3P firmware for Z80 CPU Tester developed by UR4QBP.

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 05/08/2025 | UR4QBP | Initial version. |
| 04/25/2026 | dbychkov | Added source code to GitHub. |
| 04/25/2026 | dbychkov | Changed VCO frequency to 800MHz for improved stability at high frequencies. |
| 04/25/2026 | dbychkov | Refactored I2C wait macros in Si5351_i2c.h. |
| 04/25/2026 | dbychkov | Added power-on I2C check&reset to main.c to handle SI5351A startup issues. |
| 04/25/2026 | dbychkov | Added PLL lock wait function to main.c to ensure stable clock output before releasing Z80 reset. |
| 04/25/2026 | dbychkov | Added comments and documentation for clarity. |
| 04/25/2026 | dbychkov | Standardized indentation to 4-space across all root source files. |
| 04/25/2026 | dbychkov | Translated all Russian comments to English in main.c and Si5351_i2c.h. |
| 04/25/2026 | dbychkov | Added firmware version macros (FIRMWARE_VERSION_MAJOR/MINOR) to main.h. |

## Building and Publishing Releases

### Overview

The project includes two PowerShell scripts for building firmware releases:

1. **`publish-release-stp.ps1`** – Recommended script that reads the STVD project file (.stp) and invokes the COSMIC toolchain directly. Automatically detects source files and compiler settings from the project.
2. **`publish-release.ps1`** – Original hardcoded script for reference/legacy support.

### The publish-release-stp.ps1 Script

This script builds Release_CC (common-cathode) and Release_CA (common-anode) firmware variants directly from the STVD project configuration without requiring the STVD GUI IDE.

#### Key Features

- **Reads STVD project file** (`z80_tester.stp`): Extracts compiler flags, source files, include paths, and output directories per configuration
- **Dynamic compiler flags**: Derives `-dDISPLAY_COMMON_ANODE` and other flags from the .stp file; automatically picks up changes if STVD project settings are modified
- **Auto-discovers new source files**: Any .c files added to the STVD project are automatically compiled and linked
- **Direct toolchain execution**: Uses the COSMIC STM8 compiler (`cxstm8.exe`), linker (`clnk.exe`), and hex converter (`chex.exe`) instead of invoking the STVD IDE
- **Git integration**: Creates version tags, pushes to repository, and publishes releases to GitHub
- **Flexible build modes**: Can build only, publish only, or skip specific steps

#### Prerequisites

1. **COSMIC STM8 Toolchain** – Standard installation at `C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8\`
   - Contains: `cxstm8.exe`, `clnk.exe`, `chex.exe`
   - Can be overridden via `-CosmicBinPath` parameter

2. **Git CLI** – Required for repository management and tagging

3. **GitHub CLI (`gh`)** – Optional; required only if publishing to GitHub (not needed with `-SkipPublish`)

#### Usage Examples

```powershell
# Full release: build both configs, create version tag, push to GitHub
.\build\publish-release-stp.ps1

# Build only (no git/GitHub operations)
.\build\publish-release-stp.ps1 -SkipPublish

# Publish only (skip build, use existing artifacts)
.\build\publish-release-stp.ps1 -SkipBuild

# Preflight check only (validate git state, COSMIC tools, project file)
.\build\publish-release-stp.ps1 -Preflight

# Allow uncommitted changes in working directory
.\build\publish-release-stp.ps1 -AllowDirty

# Use a custom version (instead of auto-deriving from main.h)
.\build\publish-release-stp.ps1 -Version v1.2.3

# Specify custom COSMIC toolchain location
.\build\publish-release-stp.ps1 -CosmicBinPath C:\custom\cosmic\bin
```

#### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `-SkipPublish` | Switch | False | Skip all git operations (tag, push) and GitHub release |
| `-SkipBuild` | Switch | False | Skip build; publish only (requires existing artifacts) |
| `-Preflight` | Switch | False | Validate environment and exit without building |
| `-AllowDirty` | Switch | False | Allow uncommitted changes in working directory |
| `-Version` | String | Auto (from main.h) | Custom version string (e.g., `v1.2.3`) |
| `-CosmicBinPath` | String | Auto-discovered | Path to COSMIC toolchain bin directory |

#### Workflow

1. **Preflight checks**:
   - Validates git repository state (clean working directory unless `-AllowDirty`)
   - Resolves COSMIC toolchain location
   - Parses `z80_tester.stp` project file
   - Auto-derives firmware version from `main.h` (unless `-Version` specified)

2. **Build phase** (unless `-SkipBuild`):
   - Extracts per-configuration compiler flags, include paths, and source file list from .stp
   - For each configuration (Release_CC, Release_CA):
     - Compiles all source files using `cxstm8.exe` with configuration-specific flags
     - Generates linker command file (.lkf) from source list
     - Links object files using `clnk.exe`
     - Converts to S19 hex format using `chex.exe`
   - Output artifacts:
     - `Release_CC\z80_tester_cc.s19` (common-cathode firmware)
     - `Release_CA\z80_tester_ca.s19` (common-anode firmware)

3. **Publish phase** (unless `-SkipPublish`):
   - Creates git tag: `v<MAJOR>.<MINOR>.0` (e.g., `v1.0.0`)
   - Pushes branch and tag to remote
   - Creates GitHub release with both .s19 artifacts attached

#### Dynamic Compiler Flags

The script extracts compiler flags directly from the STVD project's `CompileCommand` for each configuration. This means:

- **Release_CC** uses: `-dDISPLAY_COMMON_ANODE=0` (common-cathode)
- **Release_CA** uses: `-dDISPLAY_COMMON_ANODE=1` (common-anode)

If you modify the project in STVD and change compiler flags, they will be picked up automatically on the next build without requiring script changes.

#### Auto-Discovery of New Source Files

When you add a new `.c` file to the STVD project:

1. The script reads the updated source file list from the .stp file
2. The new file is automatically compiled during the next build
3. The new file is automatically included in the linker command file (.lkf)
4. No script modification is needed

**Note**: Header (.h) files are not auto-compiled; they must be included via `#include` in the source files that use them. Ensure proper include paths are configured in the STVD project if headers are in subdirectories.

#### Troubleshooting

| Issue | Solution |
|-------|----------|
| "COSMIC toolchain not found" | Ensure `C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8\` exists, or specify `-CosmicBinPath` |
| "Working directory not clean" | Commit changes to git or use `-AllowDirty` |
| "z80_tester.stp not found" | Run script from the STM_Source directory (root of project) |
| "Failed to parse .stp file" | Verify `z80_tester.stp` is valid INI format; check for corruption |
| "GitHub release creation failed" | Ensure `gh` CLI is installed and authenticated; use `-SkipPublish` to skip GitHub |

#### Relationship to publish-release.ps1

The original `publish-release.ps1` script uses hardcoded COSMIC compiler and linker commands. It is maintained for backward compatibility but should not be used for new builds. Use `publish-release-stp.ps1` instead, which:

- Automatically adapts to project changes
- Reduces maintenance burden
- Ensures consistency with STVD IDE project settings

---

## Z80 Tester - Code Documentation

### Project Overview

This is the STM8S103F3P microcontroller firmware for the Z80 Tester project. It generates selectable clock frequencies (1-40 MHz) using a SI5351A PLL clock generator, displays the current frequency on a 2-digit 7-segment display, and controls the Z80 processor reset line.

---

### File Structure

#### main.c - Main Application Logic

##### Hardware Pin Mapping & Macros

- **7-Segment Display Control**:
- 2 digits (DIG_1, DIG_2), polarity selected at build time
- 7 segments (SEG_A through SEG_G) plus decimal point (SEG_DP)
- **Important**:
- Segment/digit polarity is selected by `DISPLAY_COMMON_ANODE`
- `DISPLAY_COMMON_ANODE=0` -> common cathode firmware
- `DISPLAY_COMMON_ANODE=1` -> common anode firmware

- **Z80 Reset Control**: RST_Z80 on GPIO_PIN_3 (active-low reset)
- **Button Inputs**: BTN_UP_PIN and BTN_DN_PIN on GPIOA (pull-up inputs)

##### 7-Segment Display Lookup Tables

Two lookup tables are built into the firmware:

- Common cathode digit map
- Common anode digit map

The active table is selected at compile time using `DISPLAY_COMMON_ANODE`.

##### Building Two Firmware Variants (Common Cathode + Common Anode)

The project now supports a compile-time macro:

- `DISPLAY_COMMON_ANODE=0` -> common cathode firmware
- `DISPLAY_COMMON_ANODE=1` -> common anode firmware

If the macro is not defined, firmware defaults to common cathode.

Current ST Visual Develop (STM8 Cosmic) project configurations:

1. `Debug_CC` -> common cathode, debug profile
2. `Debug_CA` -> common anode, debug profile
3. `Release_CC` -> common cathode, release profile
4. `Release_CA` -> common anode, release profile

Define mapping used by these configurations:

- `*_CC` configurations compile with `DISPLAY_COMMON_ANODE=0`
- `*_CA` configurations compile with `DISPLAY_COMMON_ANODE=1`

Output examples:

- `Release_CC/z80_tester_cc.s19`
- `Release_CA/z80_tester_ca.s19`
- `Debug_CC/z80_tester_cc_dbg.s19`
- `Debug_CA/z80_tester_ca_dbg.s19`

Note: The `.s19` files are the firmware images that should be programmed into the STM8 MCU using ST Visual Programmer (STVP).

This avoids editing source code when switching display type.

##### Global Variables

```c
freq          - Current frequency in MHz (stored in EEPROM at 0x4000)
freq_max      - Upper limit: 40 MHz
freq_min      - Lower limit: 1 MHz
t             - Display multiplexing counter (0 to 2*t_delay)
t_delay       - Multiplexing divisor
dp_flag       - Decimal point flag (1 = show decimal during Z80 reset)
status        - SI5351A PLL status register value
```

##### PowerOn_I2C_Check()

**Purpose**: Handles I2C bus lockup recovery on cold power-on.

**Problem solved**:

- On cold power-on, STM8 and SI5351A start simultaneously
- Bus glitches can cause STM8 I2C peripheral to become stuck
- Only a hardware reset clears the stuck state

**Dual-boot recovery mechanism**:

- **Boot 1** (cold start, WWDGF=0): If I2C is uninitialized, SI5351_ReadRegister() times out and triggers watchdog reset
- **Boot 2** (after reset, WWDGF=1): I2C is already initialized by Boot 1, so startup proceeds normally

##### SI5351_WaitPLLLock()

**Purpose**: Waits for SI5351A PLL to lock before releasing Z80 from reset.

**Logic**:

- Reads SI5351A status register every 100ms
- Waits for bits 5 (LOL_A) and 7 (SYS_INIT) to clear
- Retries PLL reset up to 3 times (total about 8 seconds)
- Prevents Z80 from running while clock output is unstable

##### main() - Initialization & Event Loop

**Initialization**:

1. Call PowerOn_I2C_Check() to recover from I2C lockup if needed
2. Call CPU_Set() to initialize all peripherals
3. Set Z80 reset high to hold Z80 in reset
4. Set dp_flag=1 to turn the decimal point on
5. Load saved frequency from EEPROM at 0x4000
6. Program SI5351A with the saved frequency
7. Wait for PLL lock
8. Release Z80 from reset and clear dp_flag

**Main event loop**:

- Poll button inputs continuously
- When UP button is pressed:
- Hold Z80 in reset (dp_flag=1)
- Increment frequency
- Clamp to bounds with Check_Freq()
- Write new frequency to EEPROM
- Program SI5351A
- Delay 500ms for debounce
- Release Z80 from reset
- When DOWN button is pressed, the same sequence runs but frequency is decremented

**Key behavior**: Z80 reset prevents the CPU from running during frequency changes, which keeps operation stable.

##### Check_Freq()

**Purpose**: Enforces frequency bounds.

**Implementation**: Clamps frequency to the range [freq_min=1, freq_max=40] MHz.

##### CPU_Set()

**Purpose**: Complete system initialization.

**Steps**:

1. Set CPU clock prescaler (max speed: HSI/1 = 16 MHz)
2. Initialize FLASH controller
3. Configure all GPIO outputs for the 7-segment display (7 segments + 2 digit selects)
4. Configure GPIO for the Z80 reset line
5. Configure button inputs (pull-up, no interrupt)
6. Setup Timer1 with 16000 ticks per interrupt for about 100Hz refresh
7. Enable global interrupts

##### tim1UpdateInterrupt()

**Purpose**: Timer interrupt handler (about 100 Hz) implementing dynamic display multiplexing.

**Multiplexing scheme**:

- **t = t_delay*1**: Display tens digit with leading-zero suppression
- If tens digit is 0, do not light it
- Decimal point is OFF during tens display
- **t = t_delay*2**: Display units digit
- Units digit is always shown
- Decimal point is ON if dp_flag=1 during frequency change or PLL lock
- **t > t_delay*2**: Reset counter to 0

**Why multiplexing**:

- Saves GPIO pins: 2 digit lines + 8 segment lines = 10 pins
- Avoids dedicating 16 pins for independent digit control
- Alternates between digits fast enough to avoid visible flicker

**Decimal point logic**:

- Only appears on the units digit
- Indicates frequency change in progress or PLL lock wait

##### SendSeg()

**Purpose**: Decode a byte into 7-segment control signals.

**Bit mapping**:

```text
Byte bit:  0    1    2    3    4    5    6
Segment:   A    B    C    D    E    F    G
```

**Display logic**:

- The bit-to-segment mapping is fixed (A..G bits 0..6)
- ON/OFF polarity is handled by compile-time macros selected via `DISPLAY_COMMON_ANODE`
- This keeps `SendSeg()` unchanged while supporting both display types

##### FLASH_Write()

**Purpose**: Persist frequency to EEPROM at address 0x4000.

**Critical section**:

- Must complete within the watchdog timeout window
- Requires unlock before erase/write operations
- Re-locks FLASH after the write completes

---

#### Si5351_i2c.h - SI5351A Clock Generator Driver

##### Register Definitions

- SI5351A I2C address: 0xC0 (write mode)
- Register addresses for CLK outputs (0, 1, 2), PLLs (A, B), and multisynths
- Output dividers from R_DIV_1 through R_DIV_128

##### PLL Configuration

- **SI5351_PLL_FREQ**: 800 MHz
- 900 MHz is close to the documented limit but is less stable in practice
- **XTAL_FREQ**: 25 MHz crystal frequency

##### I2C Timing & Utility Functions

- **I2C_TIMEOUT_VAL**: 10,000 iterations before timeout
- **I2C_WaitEvent()** polls for I2C events and returns FALSE on timeout
- **I2C_WAIT()** exits void functions on timeout
- **I2C_WAIT_R()** exits value-returning functions with 0 on timeout

##### SI5351_I2C_Init()

- Initializes GPIOB for SDA and SCL pins
- Sets I2C clock to 100 kHz
- Uses 7-bit addressing mode

##### SI5351_SendRegister()

Writes a single register with the sequence START -> ADDR -> REGADDR -> DATA -> STOP.

##### SI5351_SetupPLL()

**Purpose**: Configure PLL A or B with a fractional-N divider.

**Parameters**:

- mult: Integer multiplier (15-90)
- num: Numerator of fractional part
- denom: Denominator of fractional part

**Calculation**:

PLL frequency = (mult + num/denom) ? XTAL_FREQ

##### SI5351_SetupMultisynth()

**Purpose**: Configure output multisynth divider.

**Parameters**:

- synth: Multisynth output (0, 1, or 2)
- divider: Integer output divider
- rDiv: Final R divider

**Output frequency** = PLL_frequency / (divider ? R_divider)

##### SI5351_SetFrequencyX()

**Purpose**: Calculate and program PLL and multisynth settings for a target frequency.

**Algorithm**:

1. Calculate divider to keep PLL near 800 MHz
2. Ensure divider is even as required by hardware
3. Calculate PLL multiplier and fractional components
4. Program PLL and multisynth
5. Return the actual multiplier value

##### SI5351_SetFrequencyA()

**Purpose**: Set CLK0 output frequency with PLL A.

**Behavior**:

- Caches previous multiplier value
- Only resets PLL when the multiplier changes
- Enables CLK0 output

##### SI5351_Init()

**Purpose**: Initialize SI5351A with default configuration.

**Steps**:

1. Set PLL load capacitance for stability
2. Reset PLL A and PLL B
3. Enable CLK0 and CLK1 outputs

##### SI5351_ReadRegister()

**Purpose**: Read a single register from SI5351A using I2C repeated START.

**Sequence**: START -> ADDR-WRITE -> REG_ADDR -> repeated START -> ADDR-READ -> DATA -> NACK -> STOP

**Important**:

- Uses repeated START to avoid releasing the bus between phases
- Single-byte read requires special ACK and STOP sequencing

---

#### stm8_interrupt_vector.c - Interrupt Vector Table

##### Purpose

Defines the STM8 interrupt vector table mapping 32 interrupt sources to their handlers.

##### Key Entries

- **IRQ11**: Timer1 update interrupt -> tim1UpdateInterrupt() from main.c
- All other entries: NonHandledInterrupt()

##### Structure

- Opcode byte: 0x82 for STM8 JP instruction
- Handler pointer: 4-byte address

---

#### main.h - Main Header

Declares the external interrupt handler:

```c
@far @interrupt void tim1UpdateInterrupt(void);
```

- `@far` means a far-call with 4-byte address
- `@interrupt` marks the function as an interrupt handler

---

### Important Design Nuances

#### 1. Common Cathode 7-Segment Display

- Active-LOW segments: writing 0V turns a segment on
- Macros are inverted compared to common-anode designs
- This is critical for correct display behavior

#### 2. Dual-Boot I2C Recovery

- Handles cold-start synchronization issues between STM8 and SI5351A
- Uses watchdog reset to recover from first-boot I2C lockup
- Proceeds normally on the second boot

#### 3. Dynamic Display Multiplexing

- Runs from Timer1 interrupt at about 100 Hz
- Alternates between two digit outputs
- Saves GPIO pins at the cost of ISR complexity

#### 4. Frequency Persistence

- Saves frequency to EEPROM at 0x4000 on each change
- Survives power loss
- Reloads on startup

#### 5. Z80 Reset Control

- Releases reset only after PLL lock is confirmed
- Holds reset during frequency changes
- Uses decimal point as a visible status indicator

#### 6. SI5351A Fractional-N PLL

- Provides 1 MHz granularity across the 1-40 MHz range
- Uses an 800 MHz VCO target for improved stability
- Uses fractional divider math for precise output generation

---

### Typical Boot Sequence

1. Power on STM8 and SI5351A
2. Run PowerOn_I2C_Check()
3. Initialize GPIO and Timer1 in CPU_Set()
4. Load frequency from EEPROM and program SI5351A
5. Wait for PLL lock in SI5351_WaitPLLLock()
6. Release Z80 reset
7. Enter main loop and monitor buttons

---

### Pin Usage Summary

| Pin | Function | Direction | Notes |
|-----|----------|-----------|-------|
| GPIOA.1 | BTN_DN | Input | Pull-up enabled |
| GPIOA.2 | BTN_UP | Input | Pull-up enabled |
| GPIOA.3 | RST_Z80 | Output | Active-low reset |
| GPIOB.4 | I2C_SCL | I/O | Open-drain |
| GPIOB.5 | I2C_SDA | I/O | Open-drain |
| GPIOC.4 | SEG_DP | Output | Decimal point (active-low) |
| GPIOC.5 | DIG_2 | Output | Digit 2 select (active-high) |
| GPIOC.6 | SEG_C | Output | Segment C (active-low) |
| GPIOC.7 | SEG_A | Output | Segment A (active-low) |
| GPIOD.1 | DIG_1 | Output | Digit 1 select (active-high) |
| GPIOD.2 | SEG_B | Output | Segment B (active-low) |
| GPIOD.3 | SEG_G | Output | Segment G (active-low) |
| GPIOD.4 | SEG_D | Output | Segment D (active-low) |
| GPIOD.5 | SEG_E | Output | Segment E (active-low) |
| GPIOD.6 | SEG_F | Output | Segment F (active-low) |


