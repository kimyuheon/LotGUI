#pragma once

#include <optional>
#include <string>

namespace lotui::example {

struct HardwareReadings {
    std::optional<int> cpuTemperature;
    std::optional<int> gpuTemperature;
    std::optional<int> gpuLoad;
    std::string cpuSource;
    std::string gpuName;
    std::string gpuSource;
};

HardwareReadings readHardwareSensors();

} // namespace lotui::example
