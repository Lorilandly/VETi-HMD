# VETi HMD - OpenVR Driver

An OpenVR/SteamVR driver for the VETi HMD. Reads orientation from a USB HID accelerometer and presents the device as a tracked HMD to SteamVR.

## Prerequisites

- CMake 3.20+
- C++17 compiler (MSVC 2019+, GCC 9+, Clang 10+)
- Git (for CMake FetchContent to pull dependencies)
- **Linux only:** `libudev-dev` (for HIDAPI hidraw backend)

Dependencies (OpenVR SDK and HIDAPI) are fetched automatically by CMake -- no manual downloads or submodules needed.

## Building

### Windows

```bash
cmake -B build -A x64
cmake --build build --config Release
```

### Linux

```bash
cmake -B build
cmake --build build --config Release
```

The built driver is placed in `build/VETi_hmd/` with the SteamVR-compatible folder structure:

```
VETi_hmd/
├── driver.vrdrivermanifest
├── bin/
│   └── win64/            (or linux64/)
│       └── driver_VETi_hmd.dll (.so on Linux)
└── resources/
    ├── icons/
    ├── input/
    ├── localization/
    ├── rendermodels/
    └── settings/
```

## Installation

Copy the `build/VETi_hmd/` folder to your SteamVR drivers directory:

```
C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\
```

Or add the path to `external_drivers` in `openvrpaths.vrpath` (located at `%LOCALAPPDATA%\openvr\openvrpaths.vrpath` on Windows):

```json
{
    "external_drivers": [
        "C:\\path\\to\\build\\VETi_hmd"
    ]
}
```

## Configuration

Driver settings are in `VETi_hmd/resources/settings/default.vrsettings`:

| Section | Key | Description |
|---------|-----|-------------|
| `driver_VETi_hmd` | `serial_number` | Device serial string |
| `driver_VETi_hmd` | `model_number` | Device model string |
| `VETi_hmd_display` | `HwId` | Monitor hardware ID for EDID and extended mode (e.g. `TDO0103`) |
| `VETi_hmd_display` | `window_width` | Display width in pixels |
| `VETi_hmd_display` | `window_height` | Display height in pixels |
| `VETi_hmd_display` | `l_display_rotation` | Left eye display rotation in degrees |
| `VETi_hmd_display` | `r_display_rotation` | Right eye display rotation in degrees |

## Project Structure

```
src/
├── DriverFactory.cpp          Entry point (HmdDriverFactory)
├── Driver/
│   ├── IVRDriver.hpp          Driver provider interface
│   ├── IVRDevice.hpp          Device interface
│   ├── DeviceType.hpp         Device type enum
│   ├── VRDriver.hpp/.cpp      IServerTrackedDeviceProvider implementation
│   ├── HMDDevice.hpp/.cpp     HMD device + IVRDisplayComponent
│   ├── EDID.hpp               EDID parsing (cross-platform)
│   └── FindMonitor.hpp        Windows Display Config API (extended mode)
└── utils/
    ├── driverlog/             DriverLog() from OpenVR samples
    └── vrmath/                Math helpers from OpenVR samples
```

## TODO

- display offset

- Chaperone

- PID proper