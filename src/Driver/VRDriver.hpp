#pragma once

#include <vector>
#include <memory>
#include <chrono>
#include <string>

#include <openvr_driver.h>
#include "IVRDriver.hpp"
#include "IVRDevice.hpp"

namespace VETiDriver {

class VRDriver : public IVRDriver {
public:
    // ── IVRDriver interface ────────────────────────────────────────────
    std::vector<std::shared_ptr<IVRDevice>> GetDevices() override;
    std::vector<vr::VREvent_t> GetOpenVREvents() override;
    std::chrono::milliseconds GetLastFrameTime() override;
    bool AddDevice(std::shared_ptr<IVRDevice> device) override;
    SettingsValue GetSettingsValue(std::string key) override;
    vr::IVRDriverInput* GetInput() override;
    vr::CVRPropertyHelpers* GetProperties() override;
    vr::IVRServerDriverHost* GetDriverHost() override;
    void Log(std::string message) override;

    // ── IServerTrackedDeviceProvider ────────────────────────────────────
    /// Called by vrserver after it receives a pointer back from HmdDriverFactory.
    /// Do resource allocations here (not in the constructor).
    vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override;

    /// Called just before the driver is unloaded from vrserver.
    void Cleanup() override;

    /// Called in the main loop of vrserver. Poll events and update devices here.
    void RunFrame() override;

    /// Returns true if the driver wants to block Standby mode.
    bool ShouldBlockStandbyMode() override;

    /// Called when the system enters a period of inactivity.
    void EnterStandby() override;

    /// Called when the system is waking up from inactivity.
    void LeaveStandby() override;

    ~VRDriver() override = default;

private:
    std::vector<std::shared_ptr<IVRDevice>> devices_;
    std::vector<vr::VREvent_t> openvr_events_;
    std::chrono::milliseconds frame_timing_ = std::chrono::milliseconds(16);
    std::chrono::system_clock::time_point last_frame_time_ = std::chrono::system_clock::now();
    std::string settings_key_ = "driver_VETi_hmd";
};

} // namespace VETiDriver
