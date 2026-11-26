#include "declarative/lotml.h"
#include "examples/platform_probe/platform_event_bridge.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"
#include "widgets/ribbon.h"

#include <charconv>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
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

struct RevealTarget {
    const lotui::Widget* widget;
    int parent;
    std::size_t ordinal;
};

std::vector<RevealTarget> loadRevealTargets(const std::filesystem::path& file,
    const lotui::declarative::LoadedUi& ui) {
    std::vector<RevealTarget> targets;
    if (file.empty()) return targets;
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read preview targets");
    int parent = -1;
    std::size_t ordinal = 0;
    std::string id;
    while (input >> parent >> ordinal >> std::quoted(id)) {
        const auto* widget = ui.find(id);
        if (!widget || parent < -1 ||
            (parent >= 0 && static_cast<std::size_t>(parent) >= targets.size()))
            throw std::runtime_error("invalid preview target");
        targets.push_back({widget, parent, ordinal});
    }
    return targets;
}

std::optional<std::size_t> revealTargetAt(
    const std::vector<RevealTarget>& targets, lotui::Point point) {
    for (auto index = targets.size(); index > 0; --index) {
        const auto& target = targets[index - 1];
        auto bounds = lotui::intersect(target.widget->bounds(), target.widget->clip());
        if (target.parent >= 0) {
            const auto* parent = targets[static_cast<std::size_t>(target.parent)].widget;
            if (const auto* ribbon = dynamic_cast<const lotui::Ribbon*>(parent)) {
                const auto header = lotui::intersect(
                    ribbon->tabHeaderBounds(target.ordinal), ribbon->clip());
                if (contains(header, point)) bounds = header;
            }
        }
        if (!contains(bounds, point)) continue;
        bool visible = true;
        for (auto branch = &target; branch->parent >= 0;) {
            const auto& parent = targets[static_cast<std::size_t>(branch->parent)];
            if (const auto* ribbon = dynamic_cast<const lotui::Ribbon*>(parent.widget);
                ribbon && ribbon->selectedIndex() != branch->ordinal &&
                !(branch == &target && contains(ribbon->tabHeaderBounds(branch->ordinal), point))) {
                visible = false;
                break;
            }
            branch = &parent;
        }
        if (visible) return index - 1;
    }
    return std::nullopt;
}

class PreviewDoubleClick {
public:
    bool advance(std::optional<std::size_t> target, lotui::Point point) {
        const auto now = std::chrono::steady_clock::now();
        const auto dx = point.x - position_.x;
        const auto dy = point.y - position_.y;
        const bool matched = target && target == lastTarget_ &&
            now - lastClick_ <= std::chrono::milliseconds(500) &&
            dx * dx + dy * dy <= 36.0F;
        lastTarget_ = target;
        lastClick_ = now;
        position_ = point;
        return matched;
    }

private:
    std::optional<std::size_t> lastTarget_;
    std::chrono::steady_clock::time_point lastClick_{};
    lotui::Point position_{};
};

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
        std::filesystem::path reveal;
        if (args.size() > fileIndex + 1 && args[fileIndex] == "--reveal") {
            reveal = args[fileIndex + 1];
            fileIndex += 2;
        }
        if (args.size() <= fileIndex || args[fileIndex].empty() || args[fileIndex].native().front() ==
                std::filesystem::path::value_type('-')) {
            std::cerr << "usage: lotml_preview [--size width height] "
                "[--capture image.ppm] [--reveal targets] file.lotml [event ...]\n";
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
        const auto targets = loadRevealTargets(reveal, ui);
        PreviewDoubleClick doubleClick;
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
                if (!targets.empty() && event.type == lotui::PlatformEventType::MouseButtonReleased &&
                    event.button == lotui::PointerButton::Primary) {
                    const lotui::Point point{event.x, event.y};
                    const auto target = revealTargetAt(targets, point);
                    if (doubleClick.advance(target, point)) {
                        std::cout << "LOTML_OPEN_EVENT " << *target << std::endl;
                        return 0;
                    }
                }
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
