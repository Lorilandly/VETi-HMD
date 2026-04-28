#pragma once

#include "IHidDevice.hpp"

namespace VETiDriver {

// HID IMU (VID 0x04D8, usage page 0xFFD8, usage 0x9F06).
//
// Report layout (report IDs enabled — buf[0] = ID, buf[1..] = payload):
//   ID 1 — Quaternion:           4 × float32 LE (quatI, quatJ, quatK, quatReal), 16 bytes
//   ID 2 — Stability classifier: 1 byte (0=Unknown … 4=Motion)
//   ID 3 — Tap detector:         1 byte flags (bit0=X+ bit1=X- bit2=Y+ bit3=Y- bit4=Z+ bit5=Z- bit6=double)
class ImuDevice final : public IHidDevice {
public:
    HidDeviceDescriptor             GetDescriptor() const override;
    std::optional<vr::DriverPose_t> OnReport(const uint8_t* data, size_t len) override;

private:
    static constexpr HidDeviceDescriptor kDesc{ 0x04D8, 0, 0xFFD8, 0x9F06 };
};

} // namespace VETiDriver
