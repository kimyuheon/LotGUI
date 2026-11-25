#include "examples/hardware_monitor_demo/sensors.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

#include <cmath>
#include <locale>
#include <memory>
#include <sstream>
#include <string_view>

namespace lotui::example {
namespace {

// NVML is optional. Load the copy installed with the NVIDIA DCH driver.
class NvidiaSensors {
public:
    NvidiaSensors() {
        library_ = LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!library_) return;
        const auto init = symbol<int (*)()>("nvmlInit_v2");
        shutdown_ = symbol<int (*)()>("nvmlShutdown");
        if (!init || !shutdown_ || init() != 0) return;
        initialized_ = true;
        const auto getDevice =
            symbol<int (*)(unsigned int, Device*)>("nvmlDeviceGetHandleByIndex_v2");
        if (!getDevice || getDevice(0, &device_) != 0 || !device_) return;
        temperature_ = symbol<TemperatureQuery>("nvmlDeviceGetTemperature");
        utilization_ = symbol<UtilizationQuery>("nvmlDeviceGetUtilizationRates");
        const auto getName = symbol<int (*)(Device, char*, unsigned int)>("nvmlDeviceGetName");
        char name[128]{};
        if (getName && getName(device_, name, sizeof(name)) == 0) name_ = name;
    }

    ~NvidiaSensors() {
        if (initialized_) shutdown_();
        if (library_) FreeLibrary(library_);
    }

    NvidiaSensors(const NvidiaSensors&) = delete;
    NvidiaSensors& operator=(const NvidiaSensors&) = delete;

    void read(HardwareReadings& values) const {
        if (!device_) return;
        values.gpuName = name_.empty() ? "NVIDIA GPU" : name_;
        values.gpuSource = "NVIDIA NVML";
        unsigned int degrees = 0;
        constexpr int gpuTemperatureSensor = 0; // NVML_TEMPERATURE_GPU
        if (temperature_ && temperature_(device_, gpuTemperatureSensor, &degrees) == 0 &&
            degrees <= 200)
            values.gpuTemperature = static_cast<int>(degrees);
        Utilization utilization{};
        if (utilization_ && utilization_(device_, &utilization) == 0 &&
            utilization.gpu <= 100)
            values.gpuLoad = static_cast<int>(utilization.gpu);
    }

private:
    struct DeviceTag;
    using Device = DeviceTag*;
    struct Utilization { unsigned int gpu, memory; };
    using TemperatureQuery = int (*)(Device, int, unsigned int*);
    using UtilizationQuery = int (*)(Device, Utilization*);

    template <typename Function> Function symbol(const char* name) const {
        // GetProcAddress returns an opaque address; the NVML signatures above
        // match the driver's C ABI.
        return reinterpret_cast<Function>(GetProcAddress(library_, name));
    }

    HMODULE library_{};
    int (*shutdown_)(){};
    bool initialized_{};
    Device device_{};
    TemperatureQuery temperature_{};
    UtilizationQuery utilization_{};
    std::string name_;
};

struct CloseInternet {
    void operator()(void* handle) const { WinHttpCloseHandle(handle); }
};
using InternetHandle = std::unique_ptr<void, CloseInternet>;

std::string localSensorMetrics() {
    InternetHandle session(WinHttpOpen(L"LotUI Hardware Monitor",
        WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session || !WinHttpSetTimeouts(session.get(), 300, 300, 300, 500))
        return {};
    InternetHandle connection(WinHttpConnect(session.get(), L"127.0.0.1", 8085, 0));
    if (!connection) return {};
    InternetHandle request(WinHttpOpenRequest(connection.get(), L"GET",
        L"/metrics", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!request ||
        !WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY,
            &redirectPolicy, sizeof(redirectPolicy)) ||
        !WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
        return {};
    DWORD status = 0;
    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE |
            WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &size, WINHTTP_NO_HEADER_INDEX) || status != 200)
        return {};

    std::string body;
    char buffer[4096];
    const ULONGLONG deadline = GetTickCount64() + 1200;
    for (;;) {
        DWORD received = 0;
        if (GetTickCount64() > deadline ||
            !WinHttpReadData(request.get(), buffer, sizeof(buffer), &received))
            return {};
        if (received == 0) return body;
        if (body.size() + received > 512 * 1024) return {};
        body.append(buffer, received);
    }
}

void readCpuPackage(std::string_view metrics, HardwareReadings& values) {
    bool foundDie = false;
    while (!metrics.empty()) {
        const auto end = metrics.find('\n');
        const auto line = metrics.substr(0, end);
        metrics = end == std::string_view::npos ? std::string_view{} : metrics.substr(end + 1);
        constexpr std::string_view prefix = "lhm_cpu_temperature_celsius ";
        if (line.substr(0, prefix.size()) != prefix) continue;
        const bool die = line.find("\"sensorName\"=\"Core (Tdie)\"") != std::string_view::npos;
        const bool package = line.find("\"sensorName\"=\"CPU Package\"") != std::string_view::npos;
        const bool combined =
            line.find("\"sensorName\"=\"Core (Tctl/Tdie)\"") != std::string_view::npos;
        const auto labelsEnd = line.rfind('}');
        if ((!die && !package && !combined) || labelsEnd == std::string_view::npos)
            continue;

        std::istringstream current(std::string(line.substr(labelsEnd + 1)));
        current.imbue(std::locale::classic());
        double degrees = 0;
        if (!(current >> degrees) || !std::isfinite(degrees) ||
            degrees < -40 || degrees > 200)
            continue;
        const auto rounded = static_cast<int>(std::lround(degrees));
        if (die && !foundDie) values.cpuTemperature.reset();
        if (die || !foundDie) {
            if (!values.cpuTemperature || rounded > *values.cpuTemperature) {
                values.cpuTemperature = rounded;
                values.cpuSource = combined ? "LibreHardwareMonitor · Tctl/Tdie" :
                    "LibreHardwareMonitor";
            }
        }
        foundDie |= die;
    }
}

} // namespace

HardwareReadings readHardwareSensors() {
    HardwareReadings values;
    static const NvidiaSensors nvidia;
    nvidia.read(values);
    readCpuPackage(localSensorMetrics(), values);
    return values;
}

} // namespace lotui::example
