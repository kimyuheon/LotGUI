#include "dialog-event.lotml.events.h"
#include "examples/platform_probe/platform_event_bridge.h"
#include "platform/platform_backend.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "text/freetype/freetype_text_engine.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

lotui::declarative::LoadedUi loadLayout(const std::filesystem::path& file,
    lotui::declarative::LoadOptions options) {
    using namespace lotui::declarative;
    LotmlLoader loader;
    loader.registry().registerWidget({"DialogHost", true, {}},
        [](const LotmlElement& element, BuildContext& context) {
            if (element.children.size() != 1)
                throw LotmlError("The dialog demo requires one root widget");
            return std::make_unique<lotui::DialogHost>(
                context.buildChild(element.children.front()));
        });

    std::ifstream input(file, std::ios::binary);
    if (!input) throw LotmlError("The dialog event layout was not found");
    std::string xml{std::istreambuf_iterator<char>(input), {}};
    if (xml.compare(0, 3, "\xEF\xBB\xBF") == 0) xml.erase(0, 3);
    const auto start = xml.find_first_not_of(" \t\r\n");
    if (start != std::string::npos && xml.compare(start, 6, "<?xml ") == 0) {
        const auto end = xml.find("?>", start);
        if (end == std::string::npos) throw LotmlError("Invalid XML declaration");
        xml.erase(start, end + 2 - start);
    }
    // The designer edits ordinary LotML. This sample supplies the modal host.
    return loader.loadString("<DialogHost>\n" + xml + "\n</DialogHost>", std::move(options));
}

void saveFrame(const lotui::VulkanRenderer::CapturedFrame& frame, const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out || frame.rgba.empty()) throw std::runtime_error("Dialog capture failed");
    out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (std::size_t i = 0; i < frame.rgba.size(); i += 4)
        out.write(reinterpret_cast<const char*>(frame.rgba.data() + i), 3);
    if (!out) throw std::runtime_error("Cannot write dialog capture");
}

int run(const std::filesystem::path& capture) {
    using namespace lotui;
    const auto resources = executableDirectory() / "resources";
    auto platform = createPlatformBackend();
    auto window = platform->createWindow({"LotML Dialog Event Demo", 620, 380, true});
    window->show();
    VulkanRenderer renderer(*window);
    auto textEngine = std::make_shared<FreetypeTextEngine>(renderer,
        std::vector<FontSource>{{"Noto Sans KR", resources / "fonts" / "NotoSansKR-Regular.ttf"},
            {"sans-serif", resources / "fonts" / "NotoSansKR-Regular.ttf"}});

    DialogHost* host = nullptr;
    declarative::LoadOptions options;
    options.textEngine = textEngine;
    designer::layout_dialog_event::bindEvents(options, host);
    auto ui = loadLayout(resources / "layouts" / "dialog-event.lotml", std::move(options));
    host = &dynamic_cast<DialogHost&>(ui.tree().root());
    const auto layout = [](WidgetTree& tree, const WindowMetrics& metrics) {
        tree.layout({0.0F, 0.0F, static_cast<float>(metrics.width),
            static_cast<float>(metrics.height)});
    };
    layout(ui.tree(), window->metrics());
    std::vector<PaintCommand> commands;
    int frames = 0;
    while (true) {
        PlatformEvent event{};
        while (window->pollEvent(event)) {
            if (!example::dispatchPlatformEvent(event, ui.tree(), *window, layout))
                return 0;
        }
        commands.clear();
        if (!capture.empty() && frames++ == 6) {
            const Widget* button = ui.find("dialogButton");
            if (!button || !hasArea(button->bounds()) || host->hasModal())
                throw std::runtime_error("Dialog did not start closed with a visible button");
            const Rect bounds = button->bounds();
            PlatformEvent click{PlatformEventType::MouseButtonPressed};
            click.x = bounds.x + bounds.width * 0.5F;
            click.y = bounds.y + bounds.height * 0.5F;
            click.button = PointerButton::Primary;
            example::dispatchPlatformEvent(click, ui.tree(), *window, layout);
            click.type = PlatformEventType::MouseButtonReleased;
            example::dispatchPlatformEvent(click, ui.tree(), *window, layout);
            if (!host->hasModal()) throw std::runtime_error("Button click did not open the dialog");
            ui.tree().paint(commands);
            saveFrame(renderer.drawFrameCaptured(commands), capture);
            PlatformEvent escape{PlatformEventType::KeyPressed};
            escape.key = KeyCode::Escape;
            example::dispatchPlatformEvent(escape, ui.tree(), *window, layout);
            if (host->hasModal()) throw std::runtime_error("Escape did not close the dialog");
            std::cout << "Dialog click and dismissal passed.\n";
            return 0;
        }
        ui.tree().paint(commands);
        renderer.drawFrame(commands);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    try {
        if (argc == 1) return run({});
        if (argc == 3 && std::filesystem::path(argv[1]) == "--capture") return run(argv[2]);
        std::cerr << "usage: lotml_dialog_demo [--capture image.ppm]\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "LotML dialog demo: " << error.what() << '\n';
        return 1;
    }
}
