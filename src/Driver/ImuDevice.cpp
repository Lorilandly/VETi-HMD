#include "ImuDevice.hpp"

namespace VETiDriver {

HidDeviceDescriptor ImuDevice::GetDescriptor() const
{
    return kDesc;
}

std::optional<vr::DriverPose_t> ImuDevice::OnReport(const uint8_t* data, size_t len)
{
    if (len < 2)
        return std::nullopt;

    switch (data[0]) {
        case 1:
            // TODO: parse 16-byte quaternion payload (quatI, quatJ, quatK, quatReal as LE float32)
            // and return a DriverPose_t with the quaternion applied.
            // Requires len >= 17 (1 ID + 16 payload).
            break;
        case 2:
            // TODO: stability classifier (0=Unknown … 4=Motion)
            // Use to gate pose.poseIsValid if needed.
            break;
        case 3:
            // TODO: tap detector flags
            // bit0=X+ bit1=X- bit2=Y+ bit3=Y- bit4=Z+ bit5=Z- bit6=double
            break;
        default:
            break;
    }

    return std::nullopt;
}

} // namespace VETiDriver
