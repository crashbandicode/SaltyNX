#pragma once

#include <array>
#include <cstdint>
#include <string_view>

// Portable config and connection policy. No SD, IPC or hardware access here.
namespace display_preferences {

inline constexpr std::array<uint8_t, 14> rates = {40, 45, 50, 55, 60, 70, 72, 75, 80, 90, 95, 100, 110, 120};
using Allowed = std::array<bool, rates.size()>;

struct Settings {
    Allowed allowed = {false, false, true, false, true};
    Allowed allowed720 = allowed;
    bool dontForce60 = false;
    bool matchLowest = false;
    bool outOfFocus60 = false;
    uint8_t preferred = 0;
    uint8_t preferred720 = 0;
};

inline std::string_view trim(std::string_view text) {
    constexpr std::string_view whitespace = " \t\r\n";
    const auto first = text.find_first_not_of(whitespace);
    if (first == text.npos) return {};
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

inline uint8_t parseRate(std::string_view value) {
    value = trim(value);
    if (value.empty() || value.size() > 3) return 0;
    unsigned number = 0;
    for (const char c : value) {
        if (c < '0' || c > '9') return 0;
        number = number * 10 + (c - '0');
    }
    for (const auto rate : rates) if (number == rate) return rate;
    return 0;
}

inline bool equalIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char c = a[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != b[i]) return false;
    }
    return true;
}

inline Allowed parseAllowed(std::string_view value) {
    Allowed defaults = Settings{}.allowed;
    Allowed result = defaults;
    value = trim(value);
    if (value.size() < 2 || value.front() != '{' || value.back() != '}') return defaults;
    value = trim(value.substr(1, value.size() - 2));
    if (value.empty()) return defaults;
    while (!value.empty()) {
        const auto comma = value.find(',');
        const auto rate = parseRate(value.substr(0, comma));
        if (!rate) return defaults;
        for (size_t i = 0; i < rates.size(); ++i) if (rates[i] == rate) result[i] = true;
        if (comma == value.npos) break;
        value = trim(value.substr(comma + 1));
        if (value.empty()) return defaults;
    }
    return result;
}

inline Settings parse(std::string_view text) {
    Settings result;
    bool common = false;
    while (!text.empty()) {
        const auto newline = text.find('\n');
        const auto line = trim(text.substr(0, newline));
        text = newline == text.npos ? std::string_view{} : text.substr(newline + 1);
        if (line.empty() || line.front() == ';' || line.front() == '#') continue;
        if (line.front() == '[') {
            common = line == "[Common]";
            continue;
        }
        if (!common) continue;
        const auto equals = line.find('=');
        if (equals == line.npos) continue;
        const auto key = trim(line.substr(0, equals));
        const auto value = trim(line.substr(equals + 1));
        if (key == "refreshRateAllowed") result.allowed = parseAllowed(value);
        else if (key == "refreshRateAllowed720p") result.allowed720 = parseAllowed(value);
        else if (key == "preferredRefreshRate") result.preferred = parseRate(value);
        else if (key == "preferredRefreshRate720p") result.preferred720 = parseRate(value);
        else if (key == "allowPatchesToForce60InDocked") result.dontForce60 = equalIgnoreCase(value, "false");
        else if (key == "matchLowestRefreshRate") result.matchLowest = equalIgnoreCase(value, "true");
        else if (key == "bringDefaultRefreshRateWhenOutOfFocus") result.outOfFocus60 = equalIgnoreCase(value, "true");
    }
    return result;
}

struct Link {
    uint32_t edid = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t rate = 0;
    uint8_t lanes = 0;
    bool operator==(const Link&) const = default;
};

inline uint8_t preferredTarget(const Settings& settings, const Link& link, uint8_t highest) {
    const bool hd = link.width == 1280 && link.height == 720;
    if (!hd && !(link.width == 1920 && link.height == 1080)) return 0;
    const uint8_t preferred = hd ? settings.preferred720 : settings.preferred;
    if (!preferred || preferred > highest) return 0;
    const auto& allowed = hd ? settings.allowed720 : settings.allowed;
    for (size_t i = 0; i < rates.size(); ++i) {
        if (preferred == rates[i] && allowed[i]) return preferred;
    }
    return 0;
}

// A mode's refresh rate is deliberately NOT part of the connection identity.
// Requiring a stable link prevents acting on transient dock-training defaults.
class Connection {
public:
    enum class Observation { Waiting, Activated, Ready };
    static constexpr uint64_t settleMs = 1000;
    static constexpr uint64_t resumeGapMs = 2000;
    static constexpr uint64_t retryMs = 1000;
    static constexpr unsigned maxAttempts = 3;

    Observation observe(const Link& next, uint64_t now) {
        if (haveCandidate && (now < lastSeen || now - lastSeen > resumeGapMs)) disconnect();
        lastSeen = now;
        if (!haveCandidate || !(next == link)) {
            link = next;
            haveCandidate = true;
            stableSince = now;
            ready_ = false;
            pending = true;
            attempts = 0;
            return Observation::Waiting;
        }
        if (!ready_ && now - stableSince >= settleMs) {
            ready_ = true;
            return Observation::Activated;
        }
        return ready_ ? Observation::Ready : Observation::Waiting;
    }

    void disconnect() { *this = {}; }
    bool ready() const { return ready_; }
    const Link& current() const { return link; }
    bool shouldApply(bool displaySync, uint64_t now) const {
        return ready_ && pending && !displaySync && attempts < maxAttempts &&
            (attempts == 0 || now - lastAttempt >= retryMs);
    }
    void attempted(bool success, uint64_t now) {
        ++attempts;
        lastAttempt = now;
        if (success || attempts >= maxAttempts) pending = false;
    }
    // A successful user request takes precedence over a not-yet-applied restore.
    void manualSelection() { pending = false; }

private:
    Link link{};
    uint64_t lastSeen = 0;
    uint64_t stableSince = 0;
    uint64_t lastAttempt = 0;
    unsigned attempts = 0;
    bool haveCandidate = false;
    bool ready_ = false;
    bool pending = false;
};

} // namespace display_preferences
