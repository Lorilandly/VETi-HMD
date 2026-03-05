#include "HMDDevice.hpp"
#include "EDID.hpp"

#ifdef _WIN32
#include "FindMonitor.hpp"
#endif

#include "driverlog.h"
#include "vrmath.h"

#include <cmath>
#include <cstring>
#include <numeric>

static const char* kMainSection = "driver_VETi_hmd";
static const char* kDisplaySection = "VETi_hmd_display";

namespace VETiDriver {

// ── HMDDevice ──────────────────────────────────────────────────────────────

HMDDevice::HMDDevice(std::string serial)
    : serial_(std::move(serial))
{
    // We have our model number and serial number stored in SteamVR settings.
    // Other IVRSettings methods (to get int32, floats, bools) return the data,
    // instead of modifying, but strings are different.
    char buf[1024];
    vr::VRSettings()->GetString(kMainSection, "model_number", buf, sizeof(buf));
    model_number_ = buf;

    DriverLog("VETi HMD Model Number: %s", model_number_.c_str());
    DriverLog("VETi HMD Serial Number: %s", serial_.c_str());
}

// Called by vrserver after IServerTrackedDeviceProvider calls
// IVRServerDriverHost::TrackedDeviceAdded.
vr::EVRInitError HMDDevice::Activate(uint32_t unObjectId)
{
    device_index_ = unObjectId;

    // ── Open HID accelerometer ─────────────────────────────────────────
    hid_init();
    hid_device_info* head = hid_enumerate(0x04d8, 0x9f04);
    for (hid_device_info* dev = head; dev; dev = dev->next) {
        if (dev->product_string && wcscmp(dev->product_string, L"Accelerometer") == 0) {
            hid_.reset(hid_open_path(dev->path));
            break;
        }
    }
    hid_free_enumeration(head);

    // ── Display configuration ──────────────────────────────────────────
    DisplayConfig display{};

    char hwid_buf[9];
    vr::VRSettings()->GetString(kDisplaySection, "HwId", hwid_buf, sizeof(hwid_buf));
    std::string hwid(hwid_buf, hwid_buf + std::strlen(hwid_buf));

    bool direct_mode = vr::VRSettings()->GetBool(vr::k_pch_DirectMode_Section, vr::k_pch_DirectMode_Enable_Bool);

    if (direct_mode) {
        DriverLog("VETi HMD: starting in direct mode");
        display.on_desktop = false;
        display.window_x = -1;
        display.window_y = -1;
        display.window_width = vr::VRSettings()->GetInt32(kDisplaySection, "window_width");
        display.window_height = vr::VRSettings()->GetInt32(kDisplaySection, "window_height");
    }
#ifdef _WIN32
    else {
        Monitor monitor;
        if (getMonitor(hwid, monitor)) {
            DriverLog("VETi HMD: starting in extended mode");
            display.on_desktop = true;
            display.window_x = monitor.window_x;
            display.window_y = monitor.window_y;
            display.window_width = static_cast<int32_t>(monitor.width);
            display.window_height = static_cast<int32_t>(monitor.height);
        } else {
            DriverLog("VETi HMD: failed to find display");
            return vr::VRInitError_Init_HmdNotFound;
        }
    }
#else
    else {
        DriverLog("VETi HMD: extended mode not supported on this platform, using direct mode settings");
        display.on_desktop = false;
        display.window_x = -1;
        display.window_y = -1;
        display.window_width = vr::VRSettings()->GetInt32(kDisplaySection, "window_width");
        display.window_height = vr::VRSettings()->GetInt32(kDisplaySection, "window_height");
    }
#endif

    display.render_width = display.window_width;
    display.render_height = display.window_height;
    display.l_display_rotation = vr::VRSettings()->GetFloat(kDisplaySection, "l_display_rotation");
    display.r_display_rotation = vr::VRSettings()->GetFloat(kDisplaySection, "r_display_rotation");

    display_component_ = std::make_unique<HMDDisplayComponent>(display);

    // ── Properties ─────────────────────────────────────────────────────
    // Properties are stored in containers, usually one per device index.
    auto container = vr::VRProperties()->TrackedDeviceToPropertyContainer(device_index_);

    // Set EDID vendor/product so SteamVR can identify the physical display
    EDIDInfo edid = ParseEDIDFromHardwareID(hwid);
    vr::VRProperties()->SetInt32Property(container, vr::Prop_EdidVendorID_Int32, edid.vendorId);
    vr::VRProperties()->SetInt32Property(container, vr::Prop_EdidProductID_Int32, edid.productId);

    vr::VRProperties()->SetStringProperty(container, vr::Prop_ModelNumber_String, model_number_.c_str());

    // Get the IPD of the user from SteamVR settings
    float ipd = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_IPD_Float);
    vr::VRProperties()->SetFloatProperty(container, vr::Prop_UserIpdMeters_Float, ipd);

    // For HMDs, a refresh rate must be set otherwise VRCompositor will fail to start.
    vr::VRProperties()->SetFloatProperty(container, vr::Prop_DisplayFrequency_Float, 0.f);

    // The distance from the user's eyes to the display in meters, used for reprojection.
    vr::VRProperties()->SetFloatProperty(container, vr::Prop_UserHeadToEyeDepthMeters_Float, 0.f);

    // How long from compositor submit to the time it takes to display on screen.
    vr::VRProperties()->SetFloatProperty(container, vr::Prop_SecondsFromVsyncToPhotons_Float, 0.11f);

    // Avoid "not fullscreen" warnings from vrmonitor
    vr::VRProperties()->SetBoolProperty(container, vr::Prop_IsOnDesktop_Bool, display.on_desktop);
    vr::VRProperties()->SetBoolProperty(container, vr::Prop_DisplayDebugMode_Bool, false);

    // This tells the UI what to show the user for bindings for this device.
    // The wildcard {<driver_name>} matches the root folder location of our driver.
    vr::VRProperties()->SetStringProperty(container, vr::Prop_InputProfilePath_String, "{VETi_hmd}/input/veti_hmd_profile.json");

    // Named icon paths for SteamVR status panel
    vr::VRProperties()->SetStringProperty(container, vr::Prop_NamedIconPathDeviceReady_String,        "{VETi_hmd}/icons/hmd_ready.png");
    vr::VRProperties()->SetStringProperty(container, vr::Prop_NamedIconPathDeviceOff_String,           "{VETi_hmd}/icons/hmd_not_ready.png");
    vr::VRProperties()->SetStringProperty(container, vr::Prop_NamedIconPathDeviceSearching_String,     "{VETi_hmd}/icons/hmd_not_ready.png");
    vr::VRProperties()->SetStringProperty(container, vr::Prop_NamedIconPathDeviceSearchingAlert_String,"{VETi_hmd}/icons/hmd_not_ready.png");
    vr::VRProperties()->SetStringProperty(container, vr::Prop_NamedIconPathDeviceReadyAlert_String,    "{VETi_hmd}/icons/hmd_not_ready.png");
    vr::VRProperties()->SetStringProperty(container, vr::Prop_NamedIconPathDeviceNotReady_String,      "{VETi_hmd}/icons/hmd_not_ready.png");
    vr::VRProperties()->SetStringProperty(container, vr::Prop_NamedIconPathDeviceStandby_String,       "{VETi_hmd}/icons/hmd_not_ready.png");
    vr::VRProperties()->SetStringProperty(container, vr::Prop_NamedIconPathDeviceAlertLow_String,      "{VETi_hmd}/icons/hmd_not_ready.png");

    // ── Input ──────────────────────────────────────────────────────────
    vr::VRDriverInput()->CreateBooleanComponent(container, "/input/system/touch",
        &input_handles_[static_cast<size_t>(HMDInput::SystemTouch)]);
    vr::VRDriverInput()->CreateBooleanComponent(container, "/input/system/click",
        &input_handles_[static_cast<size_t>(HMDInput::SystemClick)]);

    // ── Start pose thread ──────────────────────────────────────────────
    is_active_ = true;
    vr::VRServerDriverHost()->TrackedDevicePoseUpdated(device_index_, GetPose(), sizeof(vr::DriverPose_t));
    pose_update_thread_ = std::thread(&HMDDevice::PoseUpdateThread, this);

    return vr::VRInitError_None;
}

// Called by vrserver when the device should deactivate, typically at end of session.
// The device should free any resources it has allocated here.
void HMDDevice::Deactivate()
{
    if (is_active_.exchange(false))
        pose_update_thread_.join();
    device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

// Called by vrserver when the device should enter standby mode.
// The device should be put into whatever low power mode it has.
void HMDDevice::EnterStandby()
{
    DriverLog("VETi HMD: entering standby");
}

// If you're an HMD, this is where you return an implementation of
// vr::IVRDisplayComponent, vr::IVRVirtualDisplay, or vr::IVRDirectModeComponent.
void* HMDDevice::GetComponent(const char* pchComponentNameAndVersion)
{
    if (std::strcmp(pchComponentNameAndVersion, vr::IVRDisplayComponent_Version) == 0)
        return display_component_.get();
    return nullptr;
}

// Called by vrserver when a debug request has been made from an application.
void HMDDevice::DebugRequest(const char*, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

// This interface is unused in recent OpenVR versions, but is useful for
// giving data to vr::VRServerDriverHost::TrackedDevicePoseUpdated.
vr::DriverPose_t HMDDevice::GetPose()
{
    std::lock_guard<std::mutex> lock(pose_mutex_);
    return latest_pose_;
}

void HMDDevice::Update()
{
    // Per-frame input updates would go here
}

std::string HMDDevice::GetSerial() { return serial_; }
vr::TrackedDeviceIndex_t HMDDevice::GetDeviceIndex() { return device_index_; }
DeviceType HMDDevice::GetDeviceType() { return DeviceType::HMD; }

// ── Pose thread ────────────────────────────────────────────────────────────

void HMDDevice::PoseUpdateThread()
{
    std::deque<double> rolls, pitches, yaws;

    while (is_active_) {
        float accel[3];
        if (hid_ && hid_read(hid_.get(), reinterpret_cast<unsigned char*>(accel), sizeof(accel)) > 0) {
            double x = accel[0], y = accel[1], z = accel[2];
            double mag = std::sqrt(x * x + y * y + z * z);

            constexpr double kMinMagnitude = 1e-6;
            if (mag < kMinMagnitude) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            double roll  = M_PI / 2.0 - std::acos(x / mag);
            double pitch = M_PI / 2.0 - std::acos(y / mag);
            double yaw   = roll;

            rolls.push_back(roll);
            pitches.push_back(pitch);
            yaws.push_back(yaw);
            if (rolls.size() > 5) {
                rolls.pop_front();
                pitches.pop_front();
                yaws.pop_front();
            }

            roll  = std::accumulate(rolls.begin(),   rolls.end(),   0.0) / static_cast<double>(rolls.size());
            pitch = std::accumulate(pitches.begin(), pitches.end(), 0.0) / static_cast<double>(pitches.size());
            yaw   = std::accumulate(yaws.begin(),    yaws.end(),    0.0) / static_cast<double>(yaws.size());

            vr::DriverPose_t pose = { 0 };
            pose.qWorldFromDriverRotation.w = 1.0;
            pose.qDriverFromHeadRotation.w  = 1.0;
            pose.qRotation = HmdQuaternion_FromEulerAngles(roll, pitch, yaw);
            pose.vecPosition[0] = 0.0;
            pose.vecPosition[1] = 1.0;
            pose.vecPosition[2] = 0.0;
            pose.poseIsValid = true;
            pose.deviceIsConnected = true;
            pose.result = vr::TrackingResult_Running_OK;
            pose.shouldApplyHeadModel = true;

            {
                std::lock_guard<std::mutex> lock(pose_mutex_);
                latest_pose_ = pose;
            }

            vr::VRServerDriverHost()->TrackedDevicePoseUpdated(device_index_, GetPose(), sizeof(vr::DriverPose_t));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// ── HMDDisplayComponent ───────────────────────────────────────────────────

HMDDisplayComponent::HMDDisplayComponent(const DisplayConfig& config)
    : config_(config)
{
}

void HMDDisplayComponent::GetWindowBounds(int32_t* pnX, int32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight)
{
    *pnX = config_.window_x;
    *pnY = config_.window_y;
    *pnWidth = static_cast<uint32_t>(config_.window_width);
    *pnHeight = static_cast<uint32_t>(config_.window_height);
}

bool HMDDisplayComponent::IsDisplayOnDesktop()
{
    return config_.on_desktop;
}

bool HMDDisplayComponent::IsDisplayRealDisplay()
{
    return true;
}

void HMDDisplayComponent::GetRecommendedRenderTargetSize(uint32_t* pnWidth, uint32_t* pnHeight)
{
    *pnWidth = static_cast<uint32_t>(config_.render_width);
    *pnHeight = static_cast<uint32_t>(config_.render_height);
}

void HMDDisplayComponent::GetEyeOutputViewport(vr::EVREye eEye, uint32_t* pnX, uint32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight)
{
    *pnY = 0;
    *pnWidth = static_cast<uint32_t>(config_.window_width) / 2;
    *pnHeight = static_cast<uint32_t>(config_.window_height);
    *pnX = (eEye == vr::Eye_Left) ? 0 : static_cast<uint32_t>(config_.window_width) / 2;
}

void HMDDisplayComponent::GetProjectionRaw(vr::EVREye, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom)
{
    *pfLeft = -1.0f;
    *pfRight = 1.0f;
    *pfTop = -1.0f;
    *pfBottom = 1.0f;
}

vr::DistortionCoordinates_t HMDDisplayComponent::ComputeDistortion(vr::EVREye eEye, float fU, float fV)
{
    double angle = (eEye == vr::Eye_Left)
        ? DEG_TO_RAD(config_.l_display_rotation)
        : DEG_TO_RAD(config_.r_display_rotation);

    float u = fU - 0.5f;
    float v = fV - 0.5f;
    float cosA = static_cast<float>(cos(angle));
    float sinA = static_cast<float>(sin(angle));

    float ru = u * cosA - v * sinA + 0.5f;
    float rv = 1.0f - (u * sinA + v * cosA + 0.5f); // vertical flip

    vr::DistortionCoordinates_t coords{};
    coords.rfRed[0]   = coords.rfGreen[0] = coords.rfBlue[0] = ru;
    coords.rfRed[1]   = coords.rfGreen[1] = coords.rfBlue[1] = rv;
    return coords;
}

bool HMDDisplayComponent::ComputeInverseDistortion(vr::HmdVector2_t*, vr::EVREye, uint32_t, float, float)
{
    return false;
}

} // namespace VETiDriver
