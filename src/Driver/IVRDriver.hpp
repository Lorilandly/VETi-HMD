#pragma once

#include <vector>
#include <memory>
#include <variant>
#include <string>
#include <chrono>

#include <openvr_driver.h>
#include "IVRDevice.hpp"

namespace VETiDriver {

using SettingsValue = std::variant<int32_t, float, bool, std::string>;

class IVRDriver : protected vr::IServerTrackedDeviceProvider {
public:
    /// Returns all devices being managed by this driver
    virtual std::vector<std::shared_ptr<IVRDevice>> GetDevices() = 0;

    /// Returns all OpenVR events that happened on the current frame
    virtual std::vector<vr::VREvent_t> GetOpenVREvents() = 0;

    /// Returns the milliseconds between last frame and this frame
    virtual std::chrono::milliseconds GetLastFrameTime() = 0;

    /// Adds a device to the driver and registers it with SteamVR
    virtual bool AddDevice(std::shared_ptr<IVRDevice> device) = 0;

    /// Returns the value of a settings key from the driver section
    virtual SettingsValue GetSettingsValue(std::string key) = 0;

    /// Gets the OpenVR VRDriverInput pointer
    virtual vr::IVRDriverInput* GetInput() = 0;

    /// Gets the OpenVR VRDriverProperties pointer
    virtual vr::CVRPropertyHelpers* GetProperties() = 0;

    /// Gets the OpenVR VRServerDriverHost pointer
    virtual vr::IVRServerDriverHost* GetDriverHost() = 0;

    /// Writes a log message to the SteamVR driver log
    virtual void Log(std::string message) = 0;

    virtual const char* const* GetInterfaceVersions() override {
        return vr::k_InterfaceVersions;
    }

    virtual ~IVRDriver() = default;
};

} // namespace VETiDriver
