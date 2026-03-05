#include "Driver/VRDriver.hpp"
#include <memory>
#include <cstring>

#if defined(_WIN32)
#define HMD_DLL_EXPORT extern "C" __declspec(dllexport)
#elif defined(__GNUC__) || defined(__APPLE__)
#define HMD_DLL_EXPORT extern "C" __attribute__((visibility("default")))
#else
#error "Unsupported platform"
#endif

static std::shared_ptr<VETiDriver::VRDriver> driver;

HMD_DLL_EXPORT void* HmdDriverFactory(const char* pInterfaceName, int* pReturnCode)
{
    if (std::strcmp(pInterfaceName, vr::IServerTrackedDeviceProvider_Version) == 0) {
        if (!driver)
            driver = std::make_shared<VETiDriver::VRDriver>();
        return driver.get();
    }

    if (pReturnCode)
        *pReturnCode = vr::VRInitError_Init_InterfaceNotFound;
    return nullptr;
}

namespace VETiDriver {
    std::shared_ptr<VRDriver> GetDriver() { return driver; }
}
