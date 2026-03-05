#include "VRDriver.hpp"
#include "HMDDevice.hpp"
#include "driverlog.h"

namespace VETiDriver {

vr::EVRInitError VRDriver::Init(vr::IVRDriverContext* pDriverContext)
{
    VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);

    Log("Activating VETi HMD Driver...");

    AddDevice(std::make_shared<HMDDevice>("VETi_HMD"));

    Log("VETi HMD Driver loaded successfully");
    return vr::VRInitError_None;
}

void VRDriver::Cleanup()
{
    devices_.clear();
}

void VRDriver::RunFrame()
{
    vr::VREvent_t event{};
    std::vector<vr::VREvent_t> events;
    while (vr::VRServerDriverHost()->PollNextEvent(&event, sizeof(event)))
        events.push_back(event);
    openvr_events_ = std::move(events);

    auto now = std::chrono::system_clock::now();
    frame_timing_ = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_time_);
    last_frame_time_ = now;

    for (auto& device : devices_)
        device->Update();
}

bool VRDriver::ShouldBlockStandbyMode() { return false; }
void VRDriver::EnterStandby() {}
void VRDriver::LeaveStandby() {}

std::vector<std::shared_ptr<IVRDevice>> VRDriver::GetDevices() { return devices_; }
std::vector<vr::VREvent_t> VRDriver::GetOpenVREvents() { return openvr_events_; }
std::chrono::milliseconds VRDriver::GetLastFrameTime() { return frame_timing_; }

bool VRDriver::AddDevice(std::shared_ptr<IVRDevice> device)
{
    vr::ETrackedDeviceClass device_class;
    switch (device->GetDeviceType()) {
        case DeviceType::HMD:                device_class = vr::TrackedDeviceClass_HMD; break;
        case DeviceType::CONTROLLER:         device_class = vr::TrackedDeviceClass_Controller; break;
        case DeviceType::TRACKER:            device_class = vr::TrackedDeviceClass_GenericTracker; break;
        case DeviceType::TRACKING_REFERENCE: device_class = vr::TrackedDeviceClass_TrackingReference; break;
        default: return false;
    }

    bool result = vr::VRServerDriverHost()->TrackedDeviceAdded(
        device->GetSerial().c_str(), device_class, device.get());
    if (result)
        devices_.push_back(device);
    return result;
}

SettingsValue VRDriver::GetSettingsValue(std::string key)
{
    vr::EVRSettingsError err;
    int int_val = vr::VRSettings()->GetInt32(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::VRSettingsError_None) return int_val;
    float float_val = vr::VRSettings()->GetFloat(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::VRSettingsError_None) return float_val;
    bool bool_val = vr::VRSettings()->GetBool(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::VRSettingsError_None) return bool_val;
    char buf[1024];
    vr::VRSettings()->GetString(settings_key_.c_str(), key.c_str(), buf, sizeof(buf), &err);
    if (err == vr::VRSettingsError_None) return std::string(buf);
    return SettingsValue();
}

void VRDriver::Log(std::string message)
{
    DriverLog("%s", message.c_str());
}

vr::IVRDriverInput* VRDriver::GetInput() { return vr::VRDriverInput(); }
vr::CVRPropertyHelpers* VRDriver::GetProperties() { return vr::VRProperties(); }
vr::IVRServerDriverHost* VRDriver::GetDriverHost() { return vr::VRServerDriverHost(); }

} // namespace VETiDriver
