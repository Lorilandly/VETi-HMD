#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace VETiDriver {

// Matching criteria for a single HID interface.
// A value of 0 in pid, usage_page, or usage means "do not filter on that field".
struct HidDeviceDescriptor {
    uint16_t vid;
    uint16_t pid;         // 0 = any PID
    uint16_t usage_page;  // 0 = any usage page
    uint16_t usage;       // 0 = any usage
};

// HidReader owns the reconnect thread and delivers raw HID reports to callers.
//
// Usage:
//   1. Build a list of Candidate entries, each pairing a HidDeviceDescriptor with
//      an on_report callback.  Candidates are evaluated in order; the first matching
//      connected device "wins" for that connect cycle.
//   2. Construct HidReader with those candidates and an optional on_connect_change.
//   3. Call Start().  A background thread runs the outer/inner reconnect loop.
//   4. Call Stop() (or let the destructor do it) to shut down cleanly.
//
// Thread model:
//   - Outer loop: FindDevice → hid_open_path → inner loop → on error: sleep 500 ms → repeat
//   - Inner loop: hid_read_timeout(200 ms) → dispatch on_report → check running_ → repeat
//   - on_report and on_connect_change are called from the reader thread.
//
// Report-ID handling is intentionally left to the caller's on_report:
//   - Devices without report IDs: buf[0..n) is the raw payload.
//   - Devices with report IDs:    buf[0] == report ID, buf[1..n) == payload.
class HidReader {
public:
    using ReportCallback  = std::function<void(const uint8_t* data, size_t len)>;
    using ConnectCallback = std::function<void(bool connected)>;

    struct Candidate {
        HidDeviceDescriptor desc;
        ReportCallback      on_report;
    };

    explicit HidReader(std::vector<Candidate> candidates,
                       ConnectCallback        on_connect_change = {});
    ~HidReader();

    HidReader(const HidReader&)            = delete;
    HidReader& operator=(const HidReader&) = delete;

    // Starts the background reconnect thread. Must not be called more than once
    // without an intervening Stop().
    void Start();

    // Signals the thread to stop and blocks until it exits.
    void Stop();

    bool IsConnected() const { return connected_.load(); }

private:
    struct MatchResult {
        std::string      path;
        const Candidate* candidate;  // non-owning pointer into candidates_
    };

    void                       ThreadFunc();
    std::optional<MatchResult> FindDevice() const;

    std::vector<Candidate> candidates_;
    ConnectCallback        on_connect_change_;
    std::atomic<bool>      running_{false};
    std::atomic<bool>      connected_{false};
    std::thread            thread_;
};

} // namespace VETiDriver
