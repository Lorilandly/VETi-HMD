#pragma once

#include <string>
#include <array>
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>
#include <cstdint>

#include <openvr_driver.h>

#include "IVRDevice.hpp"
#include "IHidDevice.hpp"
#include "HidReader.hpp"

namespace VETiDriver {

// ── Display configuration ──────────────────────────────────────────────────
struct DisplayConfig {
    int32_t window_x = 0;
    int32_t window_y = 0;
    int32_t window_width = 1920;
    int32_t window_height = 1080;
    int32_t render_width = 1920;
    int32_t render_height = 1080;
    bool on_desktop = false;
    double display_x_offset = 0.0;
    double display_y_offset = 0.0;
    double l_display_rotation = 0.0;
    double r_display_rotation = 0.0;
};

// ── HMD device ─────────────────────────────────────────────────────────────

enum class HMDInput {
    SystemTouch,
    SystemClick,
    COUNT
};

class HMDDevice : public IVRDevice, public vr::IVRDisplayComponent {
public:
    explicit HMDDevice(std::string serial);
    ~HMDDevice() override { Deactivate(); }

    // IVRDevice
    std::string GetSerial() override;
    void Update() override;
    vr::TrackedDeviceIndex_t GetDeviceIndex() override;
    DeviceType GetDeviceType() override;

    // ITrackedDeviceServerDriver
    vr::EVRInitError Activate(uint32_t unObjectId) override;
    void Deactivate() override;
    void EnterStandby() override;
    void* GetComponent(const char* pchComponentNameAndVersion) override;
    void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override;
    vr::DriverPose_t GetPose() override;

    // IVRDisplayComponent
    void GetWindowBounds(int32_t* pnX, int32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight) override;
    bool IsDisplayOnDesktop() override;
    bool IsDisplayRealDisplay() override;
    void GetRecommendedRenderTargetSize(uint32_t* pnWidth, uint32_t* pnHeight) override;
    void GetEyeOutputViewport(vr::EVREye eEye, uint32_t* pnX, uint32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight) override;
    void GetProjectionRaw(vr::EVREye eEye, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom) override;
    vr::DistortionCoordinates_t ComputeDistortion(vr::EVREye eEye, float fU, float fV) override;
    bool ComputeInverseDistortion(vr::HmdVector2_t* pResult, vr::EVREye eEye, uint32_t unChannel, float fU, float fV) override;

private:
    void ApplyPose(const vr::DriverPose_t& pose);
    void OnConnectChange(bool connected);

    std::string serial_;
    std::string model_number_;

    std::atomic<bool>     is_active_{false};
    std::atomic<uint32_t> device_index_{vr::k_unTrackedDeviceIndexInvalid};

    DisplayConfig display_config_;

    std::mutex       pose_mutex_;
    vr::DriverPose_t latest_pose_ = IVRDevice::MakeDefaultPose(true);

    std::array<vr::VRInputComponentHandle_t, static_cast<size_t>(HMDInput::COUNT)> input_handles_{};

    // devices_ must be declared before hid_reader_ — members are constructed in
    // declaration order, and hid_reader_ captures raw pointers into this vector.
    std::vector<std::unique_ptr<IHidDevice>> devices_;
    HidReader hid_reader_;
};

} // namespace VETiDriver
