#pragma once

#include "examples/hardware_monitor_demo/sensors.h"

namespace lotui::example {

// Optional read-only queries against the driver-installed NVIDIA library.
template <class Module>
class NvidiaSensors {
public:
    NvidiaSensors() {
        if (!module_) return;
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
        return module_.template symbol<Function>(name);
    }

    Module module_;
    int (*shutdown_)(){};
    bool initialized_{};
    Device device_{};
    TemperatureQuery temperature_{};
    UtilizationQuery utilization_{};
    std::string name_;
};

} // namespace lotui::example
