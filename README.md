# Nofrendo - NES Emulator for NumWorks N0120

> A high-performance, feature-packed Nintendo Entertainment System (NES) emulator for the NumWorks N0120 graphing calculator running Epsilon 20+.

[![NumWorks](https://img.shields.io/badge/Platform-NumWorks%20N0120-yellow.svg)](https://www.numworks.com)
[![Target](https://img.shields.io/badge/SoC-STM32H725%20Cortex--M7%20@%20550MHz-blue.svg)](https://www.st.com)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-green.svg)](https://www.gnu.org/licenses/gpl-2.0.html)

This project is an optimized bare-metal port of **Nofrendo 1.2.3** specifically tailored for the **NumWorks N0120** graphing calculator (STM32H725VET6 Cortex-M7 @ 550 MHz, 320×240 16-bit color LCD, OctoSPI Flash XIP).

It introduces an **interactive Multi-ROM launcher**, an **in-game OSD Pause menu**, **2x Fast-Forward**, **fullscreen 5:4 display scaling**, **5 on-the-fly retro color palettes**, and **zero-RAM XIP storage** (direct execution from external flash memory without consuming calculator RAM).

---

## 🌟 Key Features

- **Interactive Multi-ROM Launcher**:
  - Fullscreen 320×240 catalog with pagination, smooth cursor navigation, and circular scrolling.
  - Automatic ROM metadata extraction (iNES Mapper number, mapper name, PRG/CHR ROM sizes, file size in KB).
  - Clean title truncation and responsive layout.
  - Continuous game loop: exiting a game returns directly to the catalog menu.

- **Zero-RAM Waste (Flash XIP)**:
  - All embedded ROMs are stored directly in the `.rodata` section of external OctoSPI flash memory with 32-bit alignment.
  - Internal SRAM footprint (`.data` + `.bss`) is capped at only **~16.8 KB** (out of 1 MB available), regardless of the number or size of games included!

- **In-Game OSD Pause Menu**:
  - Triggered at any time by pressing **`Toolbox`**, **`Var`**, or **`Shift` + `Back`**.
  - On-screen popup overlay allowing you to resume, toggle Fast-Forward, toggle Aspect Ratio, cycle color palettes, perform a soft reset, or exit to the ROM catalog.

- **Display Engine & Video Scaling**:
  - **4:3 Original Mode (256×240)**: Centered with stylized NES-themed retro side borders.
  - **Fullscreen Mode (320×240)**: Smooth 5:4 horizontal interpolation taking full advantage of the 550 MHz Cortex-M7 without frame drops.

- **5 Retro Color Palettes**:
  - **NES Original**: Classic vintage Nofrendo palette.
  - **Smooth Composite (FirebrandX)**: Balanced, authentic CRT composite colors.
  - **Arcade Vivid (Sony CXA)**: High-contrast, vibrant arcade rendering.
  - **Game Boy DMG-01**: Iconic 4-shade olive green monochrome.
  - **Black & White**: Vintage CRT television monochrome.

- **2x Fast-Forward Acceleration**:
  - Double the emulation rate (120 FPS target) on demand to speed through cutscenes, dialogs, and intros.

- **Epsilon 24+ Compatibility**:
  - Hardened against Cortex-M7 Memory Protection Unit (MPU) faults. Fully compatible with modern NumWorks Epsilon firmware versions (20.x through 24.x+).

---

## 🎮 Controls & Keybindings

### In-Game (NES Emulator)
| NumWorks Key | NES Controller | Description |
| :--- | :---: | :--- |
| **Arrow Keys** | **D-Pad** | Directional controls (Up, Down, Left, Right) |
| **Back** (`<--`) | **Button A** | Primary action / Jump |
| **OK** | **Button B** | Secondary action / Attack / Run |
| **EXE** | **Button A** | Primary action (alternative numpad mapping) |
| **Ans** | **Button B** | Secondary action (alternative numpad mapping) |
| **Shift** | **Select** | Game menu / Mode selection |
| **Backspace** (`[X]`) | **Start** | Start game / Internal NES pause |
| **Toolbox** / **Var** | **OSD Menu** | Open in-game Nofrendo Pause Menu |
| **Shift** + **Back** | **OSD Menu** | Alternative shortcut to open OSD Pause Menu |
| **tan** | **Hard Reset** | Instant hardware reset of the active game |

### In the Multi-ROM Launcher
| NumWorks Key | Action |
| :--- | :--- |
| **Up** / **Down** | Move selection cursor row by row |
| **Left** / **Right** | Previous page / Next page |
| **OK** or **EXE** | Launch selected game |
| **Home** | Exit to NumWorks OS (Epsilon) |

---

## 📥 Installation

### Method 1: NumWorks Web Uploader (Recommended)
1. Open Google Chrome or Microsoft Edge and navigate to **[my.numworks.com/apps](https://my.numworks.com/apps)**.
2. Connect your NumWorks calculator using its USB cable.
3. Drag and drop the binary file **`output/nofrendo.nwa`** onto the web page.
4. Click **Install on calculator**.

### Method 2: Command Line (nwlink)
With Node.js installed on your computer:
```bash
npx --yes -- nwlink@0.0.19 install-nwa output/nofrendo.nwa
```

---

## 🛠️ Building from Source

### Prerequisites
- **ARM Embedded Toolchain**: `arm-none-eabi-gcc` installed and in your system `PATH`.
- **Node.js**: Required for the `nwlink` packaging utility.
- **Python 3.8+**: Used for build automation and ROM embedding.

### Compile
Run the unified build script:
```bash
python build.py
```
This script handles C module compilation, application icon generation, elf linking, and packaging into `output/nofrendo.nwa` and `output/nofrendo.bin`.

---

## 🕹️ Adding Your Own NES ROMs

To customize the embedded games library:

1. Place your `.nes` ROM files into the **`roms/`** directory.
2. Regenerate the embedded C source catalog:
   ```bash
   python tools/embed_roms.py -i roms -c src/rom_catalog.c
   ```
3. Rebuild the application:
   ```bash
   python build.py
   ```
4. Flash the newly generated `output/nofrendo.nwa` onto your calculator.

---

## 📐 Technical Specifications

- **Target Device**: NumWorks N0120 (STM32H725VET6 MCU, ARM Cortex-M7 @ 550 MHz).
- **Video Output**: 320×240 RGB565 via NumWorks bare-metal EADK API.
- **ROM Storage**: External OctoSPI Flash (XIP in `.rodata`, 4-byte aligned).
- **RAM Usage**:
  - **SRAM (.data + .bss)**: ~16.8 KB (out of 1024 KB available).
  - **Flash (.text + .rodata)**: ~1.26 MB (with 7 included games).
- **Supported Mappers**: NROM (0), MMC1 (1), UNROM (2), CNROM (3), MMC3 (4), MMC5 (5), AOROM (7), GxROM (66), and over 30 additional mappers implemented in Nofrendo.

---

## 📜 License

- **Nofrendo Core**: GNU General Public License v2 (GPL-2.0).
- **NumWorks Port & Enhancements**: Open-source under MIT / GPL-2.0.
