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
    virtual std::vector<std::shared_ptr<IVRDevice>> GetDevices() = 0;
    virtual std::vector<vr::VREvent_t> GetOpenVREvents() = 0;
    virtual std::chrono::milliseconds GetLastFrameTime() = 0;

    virtual bool AddDevice(std::shared_ptr<IVRDevice> device) = 0;
    virtual SettingsValue GetSettingsValue(std::string key) = 0;

    virtual vr::IVRDriverInput* GetInput() = 0;
    virtual vr::CVRPropertyHelpers* GetProperties() = 0;
    virtual vr::IVRServerDriverHost* GetDriverHost() = 0;

    virtual void Log(std::string message) = 0;

    virtual const char* const* GetInterfaceVersions() override {
        return vr::k_InterfaceVersions;
    }

    virtual ~IVRDriver() = default;
};

} // namespace VETiDriver
