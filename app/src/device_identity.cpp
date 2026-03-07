#include "device_identity.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include <unistd.h>

namespace device_identity {

namespace {

static std::string Trim(const std::string &s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

static bool ReadFileFirstLine(const char *path, std::string &out)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string line;
    std::getline(ifs, line);
    line = Trim(line);
    // Some device-tree strings may contain trailing NULs when viewed as text.
    while (!line.empty() && line.back() == '\0') line.pop_back();
    if (line.empty()) return false;
    out = line;
    return true;
}

static bool ReadCpuInfoSerial(std::string &out)
{
    std::ifstream ifs("/proc/cpuinfo");
    if (!ifs.is_open()) return false;
    std::string line;
    while (std::getline(ifs, line)) {
        // Common formats:
        // "Serial\t\t: 00000000abcdef01"
        // "Serial\t: 1234..."
        auto pos = line.find("Serial");
        if (pos == std::string::npos) continue;
        pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string v = Trim(line.substr(pos + 1));
        if (!v.empty()) {
            out = v;
            return true;
        }
    }
    return false;
}

static std::string HostnameFallback()
{
    char buf[256];
    if (gethostname(buf, sizeof(buf) - 1) == 0) {
        buf[sizeof(buf) - 1] = '\0';
        std::string hn = Trim(std::string(buf));
        if (!hn.empty()) return hn;
    }
    return std::string("unknown");
}

} // namespace

std::string SanitizeTopicSegment(const std::string &in)
{
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        if (std::isalnum(c) || c == '_' || c == '-' || c == '.') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }
    // Collapse repeated underscores to keep it readable.
    out.erase(std::unique(out.begin(), out.end(), [](char a, char b) { return a == '_' && b == '_'; }), out.end());
    // Trim underscores.
    while (!out.empty() && out.front() == '_') out.erase(out.begin());
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty()) out = "unknown";
    return out;
}

std::string GetBestEffortChipId()
{
    std::string v;
    // 1) Device-tree serial
    if (ReadFileFirstLine("/proc/device-tree/serial-number", v)) {
        return SanitizeTopicSegment(v);
    }
    // 2) CPU info serial
    if (ReadCpuInfoSerial(v)) {
        return SanitizeTopicSegment(v);
    }
    // 3) machine-id
    if (ReadFileFirstLine("/etc/machine-id", v)) {
        return SanitizeTopicSegment(v);
    }
    if (ReadFileFirstLine("/var/lib/dbus/machine-id", v)) {
        return SanitizeTopicSegment(v);
    }
    return SanitizeTopicSegment(HostnameFallback());
}

std::string ExpandTopicWithDeviceId(const std::string &topic,
                                    const std::string &deviceId,
                                    bool expandLegacy)
{
    const std::string safeId = SanitizeTopicSegment(deviceId);
    std::string out = topic;

    auto replace_all = [](std::string &s, const std::string &from, const std::string &to) {
        if (from.empty()) return;
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    replace_all(out, "{deviceId}", safeId);
    replace_all(out, "<deviceId>", safeId);

    if (!expandLegacy) {
        return out;
    }

    // Legacy topics (no device id in topic). Expand "<prefix>/sensors" -> "<prefix>/<id>/sensors".
    // Keep it conservative: only expand when there is exactly one '/' in the topic.
    const size_t firstSlash = out.find('/');
    if (firstSlash != std::string::npos && out.find('/', firstSlash + 1) == std::string::npos) {
        const std::string prefix = out.substr(0, firstSlash);
        const std::string leaf = out.substr(firstSlash + 1);
        if (leaf == "sensors") {
            return prefix + "/" + safeId + "/sensors";
        }
        if (leaf == "control") {
            return prefix + "/" + safeId + "/control";
        }
    }
    return out;
}

} // namespace device_identity
