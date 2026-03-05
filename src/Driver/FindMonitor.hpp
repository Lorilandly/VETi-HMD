#pragma once

#ifdef _WIN32

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace VETiDriver {

// Simple structure to hold monitor data found via the Windows Display Configuration API.
struct Monitor {
    std::wstring name;      // Device name (e.g., \\.\DISPLAY1)
    std::wstring desc;      // Friendly monitor name
    std::wstring hwId;      // Hardware ID substring (vendor+product)
    int32_t window_x;
    int32_t window_y;
    uint32_t width;
    uint32_t height;
};

struct MonitorInfo {
    std::wstring description;
    std::wstring hardwareId;
};

// Get target device info (monitor name and device path) from a display path.
inline std::unique_ptr<MonitorInfo> GetMonitorInfoFromPath(const DISPLAYCONFIG_PATH_INFO& path) {
    DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
    targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
    targetName.header.size = sizeof(targetName);
    targetName.header.adapterId = path.targetInfo.adapterId;
    targetName.header.id = path.targetInfo.id;

    if (DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS)
        return nullptr;

    auto info = std::make_unique<MonitorInfo>();
    info->description = targetName.monitorFriendlyDeviceName;
    info->hardwareId = targetName.monitorDevicePath;
    return info;
}

// Search active display paths for a monitor whose hardware ID contains the
// given vendor+product string (e.g. "TDO0103"). Populates monitorOut with
// the display's position, dimensions, and friendly name.
inline bool getMonitor(const std::string& hwid, Monitor& monitorOut) {
    if (hwid.length() < 7)
        return false;

    UINT32 pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
        return false;
    if (pathCount == 0)
        return false;

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS)
        return false;

    for (const auto& path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {};
        sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        sourceName.header.size = sizeof(sourceName);
        sourceName.header.adapterId = path.sourceInfo.adapterId;
        sourceName.header.id = path.sourceInfo.id;

        if (DisplayConfigGetDeviceInfo(&sourceName.header) != ERROR_SUCCESS)
            continue;

        auto monitorInfo = GetMonitorInfoFromPath(path);
        if (!monitorInfo)
            continue;

        // Extract vendor+product from the monitor device path
        // (format: ...#<vendor+product>#...)
        size_t start = monitorInfo->hardwareId.find(L"#");
        if (start == std::wstring::npos) continue;
        size_t end = monitorInfo->hardwareId.find(L"#", start + 1);
        if (end == std::wstring::npos) continue;

        std::wstring vendorProduct = monitorInfo->hardwareId.substr(start + 1, end - start - 1);
        if (vendorProduct != std::wstring(hwid.begin(), hwid.end()))
            continue;

        monitorOut.name = sourceName.viewGdiDeviceName;
        monitorOut.desc = monitorInfo->description;
        monitorOut.hwId = vendorProduct;

        if (path.sourceInfo.modeInfoIdx >= modes.size())
            continue;

        const auto& sourceMode = modes[path.sourceInfo.modeInfoIdx].sourceMode;
        monitorOut.width = sourceMode.width;
        monitorOut.height = sourceMode.height;
        monitorOut.window_x = sourceMode.position.x;
        monitorOut.window_y = sourceMode.position.y;
        return true;
    }

    return false;
}

} // namespace VETiDriver

#endif // _WIN32
