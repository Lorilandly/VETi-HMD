#pragma once

#include <string>
#include <openvr_driver.h>
#include "DeviceType.hpp"

namespace VETiDriver {

class IVRDevice : public vr::ITrackedDeviceServerDriver {
public:
    /// Returns the serial string for this device.
    virtual std::string GetSerial() = 0;

    /// Runs any update logic for this device.
    /// Called once per frame
    virtual void Update() = 0;

    /// Returns the OpenVR device index
    /// This should be 0 for HMDs    
    virtual vr::TrackedDeviceIndex_t GetDeviceIndex() = 0;

    /// Returns which type of device this device is
    virtual DeviceType GetDeviceType() = 0;

    /// Makes a default device pose 
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
