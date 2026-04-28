#include "AccelDevice.hpp"

#include <cstring>

namespace VETiDriver {

HidDeviceDescriptor AccelDevice::GetDescriptor() const
{
    return kDesc;
}

std::optional<vr::DriverPose_t> AccelDevice::OnReport(const uint8_t* data, size_t len)
{
    if (len < sizeof(float) * 3)
        return std::nullopt;

    float accel[3];
    std::memcpy(accel, data, sizeof(accel));

    double acc_d[3] = { accel[0], accel[1], accel[2] };
    acc_filter_.update(acc_d);

    vr::DriverPose_t pose = { 0 };
    pose.qWorldFromDriverRotation.w = 1.0;
    pose.qDriverFromHeadRotation.w  = 1.0;
    pose.qRotation                  = acc_filter_.getQuat();
    pose.vecPosition[0]             = 0.0;
    pose.vecPosition[1]             = 1.0;
    pose.vecPosition[2]             = 0.0;
    pose.poseIsValid                = true;
    pose.deviceIsConnected          = true;
    pose.result                     = vr::TrackingResult_Running_OK;
    pose.shouldApplyHeadModel       = true;

    return pose;
}

} // namespace VETiDriver
