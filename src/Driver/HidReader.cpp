#include "HidReader.hpp"

#include "driverlog.h"

#include <hidapi.h>

#include <chrono>
#include <thread>

namespace VETiDriver {

// ── Construction / destruction ─────────────────────────────────────────────

HidReader::HidReader(std::vector<Candidate> candidates,
                     ConnectCallback        on_connect_change)
    : candidates_(std::move(candidates))
    , on_connect_change_(std::move(on_connect_change))
{}

HidReader::~HidReader()
{
    Stop();
}

// ── Public API ─────────────────────────────────────────────────────────────

void HidReader::Start()
{
    running_ = true;
    thread_  = std::thread(&HidReader::ThreadFunc, this);
}

void HidReader::Stop()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

// ── Private: device discovery ──────────────────────────────────────────────

// Walks the candidate list in order.  For each candidate, enumerates HID
// devices filtered by VID (and PID when non-zero), then checks usage_page and
// usage against the enumerated device (skipping the check when the descriptor
// field is zero).  Returns the path of the first matching device together with
// a pointer to its Candidate entry.
std::optional<HidReader::MatchResult> HidReader::FindDevice() const
{
    for (const Candidate& cand : candidates_) {
        const HidDeviceDescriptor& d = cand.desc;

        hid_device_info* head = hid_enumerate(d.vid, d.pid);
        for (hid_device_info* dev = head; dev; dev = dev->next) {
            if (d.usage_page != 0 && dev->usage_page != d.usage_page)
                continue;
            if (d.usage != 0 && dev->usage != d.usage)
                continue;

            std::string path(dev->path);
            hid_free_enumeration(head);
            return MatchResult{ std::move(path), &cand };
        }
        hid_free_enumeration(head);
    }
    return std::nullopt;
}

// ── Private: reconnect thread ──────────────────────────────────────────────

void HidReader::ThreadFunc()
{
    // 64 bytes is enough for the largest expected report (IMU: 1 ID + 16 payload)
    // plus comfortable headroom for future reports.
    constexpr int    kReadTimeoutMs = 200;
    constexpr size_t kBufSize       = 64;
    uint8_t          buf[kBufSize];

    hid_init();

    while (running_) {
        // ── Outer loop: discover and open ─────────────────────────────────
        auto match = FindDevice();
        if (!match) {
            if (connected_.exchange(false)) {
                DriverLog("HidReader: device not found, waiting to reconnect");
                if (on_connect_change_) on_connect_change_(false);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        hid_device* raw = hid_open_path(match->path.c_str());
        if (!raw) {
            DriverLog("HidReader: hid_open_path failed (device may have vanished), retrying");
            connected_ = false;
            if (on_connect_change_) on_connect_change_(false);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // Wrap in a unique_ptr so it's closed on every exit path below.
        std::unique_ptr<hid_device, decltype(&hid_close)> dev(raw, &hid_close);

        hid_set_nonblocking(dev.get(), 0);

        connected_ = true;
        DriverLog("HidReader: device opened (usage_page=0x%04X usage=0x%04X)",
                  match->candidate->desc.usage_page,
                  match->candidate->desc.usage);
        if (on_connect_change_) on_connect_change_(true);

        // ── Inner loop: read until error ──────────────────────────────────
        while (running_) {
            int n = hid_read_timeout(dev.get(), buf, kBufSize, kReadTimeoutMs);

            if (n < 0) {
                DriverLog("HidReader: read error — device disconnected");
                break;
            }
            if (n == 0)
                continue;  // timeout with no report; re-check running_

            if (match->candidate->on_report)
                match->candidate->on_report(buf, static_cast<size_t>(n));
        }

        // ── After inner-loop exit: mark disconnected, then re-enumerate ───
        connected_ = false;
        if (on_connect_change_) on_connect_change_(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    hid_exit();
}

} // namespace VETiDriver
