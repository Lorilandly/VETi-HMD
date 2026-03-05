#pragma once

#include <string>
#include <cstdint>

namespace VETiDriver {

// Parse EDID VID/PID from hardware ID string (e.g. "TDO0103")
// with byte swapping for consistency with other EDID tools.
struct EDIDInfo {
    uint16_t vendorId;
    uint16_t productId;
    std::string vendorCode;
};

// Extracts vendor and product IDs from a hardware ID string.
// The first 3 characters are the PNPID vendor code (A=1, B=2, ..., Z=26),
// the remaining characters are the product code in hex.
inline EDIDInfo ParseEDIDFromHardwareID(const std::string& hwId) {
    EDIDInfo edid = {0, 0, "UNK"};
    if (hwId.length() < 7) return edid;

    std::string vendor = hwId.substr(0, 3);
    std::string product = hwId.substr(3);
    edid.vendorCode = vendor;

    // Convert 3-char vendor code to EDID VID
    if (vendor.length() == 3) {
        uint16_t vid = 0;
        for (int i = 0; i < 3; i++) {
            if (vendor[i] >= 'A' && vendor[i] <= 'Z')
                vid |= ((vendor[i] - 'A' + 1) << (10 - i * 5));
        }
        edid.vendorId = ((vid & 0xFF) << 8) | ((vid >> 8) & 0xFF);
    }

    // Convert hex product code string to EDID PID
    if (!product.empty()) {
        try {
            uint16_t pid = static_cast<uint16_t>(std::stoul(product, nullptr, 16));
            edid.productId = ((pid & 0xFF) << 8) | ((pid >> 8) & 0xFF);
        } catch (...) {
            edid.productId = 0;
        }
    }

    return edid;
}

} // namespace VETiDriver
