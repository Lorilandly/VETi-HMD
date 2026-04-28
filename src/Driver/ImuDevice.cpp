#include "ImuDevice.hpp"

#include <cstring>

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
        case 1: {
            // Payload: quatI, quatJ, quatK, quatReal — each a 32-bit LE float.
            // Requires 1 (report ID) + 16 (4 × float32) = 17 bytes.
            if (len < 1 + sizeof(float) * 4)
                return std::nullopt;

            float q[4];
            std::memcpy(q, data + 1, sizeof(q));
            // q[0]=quatI, q[1]=quatJ, q[2]=quatK, q[3]=quatReal

            vr::DriverPose_t pose = { 0 };
            pose.qWorldFromDriverRotation.w = 1.0;
            pose.qDriverFromHeadRotation.w  = 1.0;
            pose.qRotation.x                =  static_cast<double>(q[0]);
            pose.qRotation.y                = -static_cast<double>(q[2]);
            pose.qRotation.z                =  static_cast<double>(q[1]);
            pose.qRotation.w                =  static_cast<double>(q[3]);
            pose.vecPosition[0]             = 0.0;
            pose.vecPosition[1]             = 1.0;
            pose.vecPosition[2]             = 0.0;
            pose.poseIsValid                = true;
            pose.deviceIsConnected          = true;
            pose.result                     = vr::TrackingResult_Running_OK;
            pose.shouldApplyHeadModel       = true;

            return pose;
        }
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
