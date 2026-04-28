#pragma once

#include <cstdint>
#include <optional>

#include <openvr_driver.h>

#include "HidReader.hpp"

namespace VETiDriver {

// Interface for a single HID sensor that can produce tracking poses.
//
// Implement GetDescriptor() to specify which HID interface to open.
// Implement OnReport() to parse raw bytes and return a pose when one is ready.
// Return std::nullopt for reports that don't produce a pose (e.g. status,
// tap, or truncated packets).
class IHidDevice {
public:
    virtual ~IHidDevice() = default;

    virtual HidDeviceDescriptor                  GetDescriptor() const = 0;
    virtual std::optional<vr::DriverPose_t>      OnReport(const uint8_t* data, size_t len) = 0;
};

} // namespace VETiDriver
