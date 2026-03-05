#pragma once

#include <string>
#include <openvr_driver.h>
#include "DeviceType.hpp"

namespace VETiDriver {

class IVRDevice : public vr::ITrackedDeviceServerDriver {
public:
    virtual std::string GetSerial() = 0;
    virtual void Update() = 0;
    virtual vr::TrackedDeviceIndex_t GetDeviceIndex() = 0;
    virtual DeviceType GetDeviceType() = 0;

    static inline vr::DriverPose_t MakeDefaultPose(bool connected = true, bool tracking = true) {
        vr::DriverPose_t pose = { 0 };
        pose.deviceIsConnected = connected;
        pose.poseIsValid = tracking;
        pose.result = tracking
            ? vr::TrackingResult_Running_OK
            : vr::TrackingResult_Running_OutOfRange;
        pose.willDriftInYaw = false;
        pose.shouldApplyHeadModel = false;
        pose.qDriverFromHeadRotation.w = 1.0;
        pose.qWorldFromDriverRotation.w = 1.0;
        pose.qRotation.w = 1.0;
        return pose;
    }

    virtual ~IVRDevice() = default;
};

} // namespace VETiDriver
