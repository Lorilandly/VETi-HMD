#pragma once

#include "IHidDevice.hpp"
#include "utils/AccelOrientationFilter.hpp"

namespace VETiDriver {

// HID accelerometer (VID 0x04D8, usage page 0xFFD8, usage 0x9F04).
// Report format: 3 × float32 (little-endian), no report-ID prefix.
// Produces an inclination-corrected quaternion pose via AccelOrientationFilter.
class AccelDevice final : public IHidDevice {
public:
    HidDeviceDescriptor             GetDescriptor() const override;
    std::optional<vr::DriverPose_t> OnReport(const uint8_t* data, size_t len) override;

private:
    static constexpr HidDeviceDescriptor kDesc{ 0x04D8, 0, 0xFFD8, 0x9F04 };
    static constexpr double kAccTau = 0.1;   // Butterworth LPF time constant (seconds)
    static constexpr double kAccTs  = 0.01;  // assumed sample period (seconds, 100 Hz)

    AccelOrientationFilter acc_filter_{ kAccTau, kAccTs };
};

} // namespace VETiDriver
