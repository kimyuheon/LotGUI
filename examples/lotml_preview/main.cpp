#include "declarative/lotml.h"
#include "examples/platform_probe/platform_event_bridge.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"

#include <charconv>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

const char* propertyType(lotui::declarative::PropertyType type) {
    using Type = lotui::declarative::PropertyType;
    switch (type) {
    case Type::String: return "string";
    case Type::Number: return "number";
    case Type::Boolean: return "boolean";
    case Type::Color: return "color";
    case Type::Insets: return "insets";
    case Type::Enumeration: return "enumeration";
    case Type::Event: return "event";
    }
    return "string";
}

void printSchema(const lotui::declarative::WidgetRegistry& registry) {
    bool firstWidget = true;
    std::cout << "[\n";
    for (const auto& descriptor : registry.descriptors()) {
        if (!firstWidget) std::cout << ",\n";
        firstWidget = false;
        std::cout << "  {\"type\":" << std::quoted(descriptor.type)
                  << ",\"allowsChildren\":"
                  << (descriptor.allowsChildren ? "true" : "false")
                  << ",\"properties\":[\n";
        bool firstProperty = true;
        for (const auto& property : descriptor.properties) {
            if (!firstProperty) std::cout << ",\n";
            firstProperty = false;
            std::cout << "    {\"name\":" << std::quoted(property.name)
                      << ",\"type\":" << std::quoted(propertyType(property.type))
                      << ",\"default\":" << std::quoted(property.defaultValue)
                      << ",\"required\":" << (property.required ? "true" : "false")
                      << "}";
        }
        std::cout << "\n  ]}";
    }
    std::cout << "\n]\n";
}

void writeCapture(const lotui::VulkanRenderer::CapturedFrame& frame,
    const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out || frame.rgba.empty()) throw std::runtime_error("capture failed");
    out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (std::size_t i = 0; i < frame.rgba.size(); i += 4)
        out.write(reinterpret_cast<const char*>(frame.rgba.data() + i), 3);
    if (!out) throw std::runtime_error("capture write failed");
}

int dimension(const std::filesystem::path& argument, int min, int max) {
    const auto text = argument.u8string();
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value < min || value > max)
        throw std::runtime_error("invalid preview size");
    return value;
}

int runPreview(const std::vector<std::filesystem::path>& args) {
    try {
        lotui::declarative::LotmlLoader loader;
        if (args.size() == 1 && args.front() == "--schema") {
            printSchema(loader.registry());
            return 0;
        }
        std::size_t fileIndex = 0;
        int width = 720;
        int height = 530;
        if (args.size() >= 3 && args.front() == "--size") {
            width = dimension(args[1], 240, 1920);
            height = dimension(args[2], 180, 1400);
            fileIndex += 3;
        }
        std::filesystem::path capture;
        if (args.size() > fileIndex + 1 && args[fileIndex] == "--capture") {
            capture = args[fileIndex + 1];
            fileIndex += 2;
        }
        if (args.size() <= fileIndex || args[fileIndex].empty() || args[fileIndex].native().front() ==
                std::filesystem::path::value_type('-')) {
            std::cerr << "usage: lotml_preview [--size width height] "
                "[--capture image.ppm] file.lotml [event ...]\n";
            return 2;
        }
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow({"LotML Preview", width, height, true});
        window->show();
        lotui::VulkanRenderer renderer(*window);
        const auto font = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        using lotui::FontSource;
        lotui::declarative::LoadOptions options;
        options.textEngine = std::make_shared<lotui::FreetypeTextEngine>(
            renderer, std::vector<FontSource>{{"Noto Sans KR", font},
                {"sans-serif", font}});
        for (std::size_t i = fileIndex + 1; i < args.size(); ++i)
            options.events.emplace(args[i].u8string(), [] {});
        auto ui = loader.loadFile(args[fileIndex], std::move(options));
        const auto layout = [](lotui::WidgetTree& tree,
            const lotui::WindowMetrics& metrics) {
            tree.layout({0, 0, static_cast<float>(metrics.width),
                static_cast<float>(metrics.height)});
        };
        layout(ui.tree(), window->metrics());
        std::vector<lotui::PaintCommand> commands;
        for (int frame = 0;;) {
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                if (!lotui::example::dispatchPlatformEvent(
                        event, ui.tree(), *window, layout))
                    return 0;
            }
            commands.clear();
            ui.tree().paint(commands);
            if (!capture.empty() && frame++ == 6) {
                writeCapture(renderer.drawFrameCaptured(commands), capture);
                return 0;
            }
            renderer.drawFrame(commands);
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    } catch (const std::exception& error) {
        std::cerr << "LotML preview: " << error.what() << '\n';
        return 1;
    }
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    return runPreview({argv + 1, argv + argc});
}
