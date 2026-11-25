#include "examples/hardware_monitor_demo/sensors_nvml.h"

#include <dlfcn.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace lotui::example {
namespace {

class NvmlModule {
public:
    NvmlModule() = default;
    ~NvmlModule() { if (library_) dlclose(library_); }
    NvmlModule(const NvmlModule&) = delete;
    NvmlModule& operator=(const NvmlModule&) = delete;
    explicit operator bool() const noexcept { return library_ != nullptr; }

    template <typename Function> Function symbol(const char* name) const {
        return reinterpret_cast<Function>(dlsym(library_, name));
    }

private:
    void* library_{dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL)};
};

std::string readLine(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    return line;
}

std::optional<int> temperature(const std::filesystem::path& path) {
    int value = 0;
    std::ifstream input(path);
    if (!(input >> value) || value < -40000 || value > 200000) return {};
    return static_cast<int>(std::lround(value / 1000.0));
}

void readPackage(const std::filesystem::path& hwmon, std::string_view chip,
    HardwareReadings& readings) {
    std::error_code error;
    for (std::filesystem::directory_iterator entry(hwmon, error), end;
            !error && entry != end; entry.increment(error)) {
        constexpr std::string_view suffix = "_label";
        const auto filename = entry->path().filename().string();
        if (filename.size() <= 4 + suffix.size() ||
            filename.compare(0, 4, "temp") != 0 ||
            filename.compare(filename.size() - suffix.size(), suffix.size(),
                suffix) != 0)
            continue;
        const auto label = readLine(entry->path());
        const bool package = chip == "coretemp" ? label.rfind("Package id ", 0) == 0
                                                : label == "Tdie";
        if (!package) continue;
        const auto input = filename.substr(0, filename.size() - suffix.size()) + "_input";
        const auto next = temperature(hwmon / input);
        if (next && (!readings.cpuTemperature || *next > *readings.cpuTemperature)) {
            readings.cpuTemperature = next;
            readings.cpuSource = "Linux hwmon · " + std::string(chip);
        }
    }
}

} // namespace

HardwareReadings readHardwareSensors() {
    HardwareReadings readings;
    static const NvidiaSensors<NvmlModule> nvidia;
    nvidia.read(readings);
    if (!readings.gpuTemperature && !readings.gpuLoad) {
        readings.gpuName.clear();
        readings.gpuSource.clear();
    }
    std::error_code error;
    for (std::filesystem::directory_iterator entry("/sys/class/hwmon", error), end;
            !error && entry != end; entry.increment(error)) {
        const auto& path = entry->path();
        const auto name = readLine(path / "name");
        if (name == "coretemp" || name == "k10temp") {
            readPackage(path, name, readings);
        } else if (name == "amdgpu" && readings.gpuName.empty()) {
            const auto temp = temperature(path / "temp1_input");
            const auto model = readLine(path / "device" / "product_name");
            std::ifstream busy(path / "device" / "gpu_busy_percent");
            int load = 0;
            if ((busy >> load) && load >= 0 && load <= 100) readings.gpuLoad = load;
            if (!temp && !readings.gpuLoad) continue;
            readings.gpuTemperature = temp;
            readings.gpuName = model.empty() ? "AMD GPU" : model;
            readings.gpuSource = "Linux hwmon · amdgpu";
        }
    }
    return readings;
}

} // namespace lotui::example
