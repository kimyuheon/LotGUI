#include "core/widget_tree.h"
#include "platform/platform_backend.h"
#include "platform/platform_event.h"
#include "platform/runtime_paths.h"
#include "renderer/vulkan/vulkan_renderer.h"
#ifdef LOTUI_HAS_EXAMPLE_TEXT
#include "text/freetype/freetype_text_engine.h"
#endif
#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/numeric_input.h"
#include "widgets/text_field.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <cmath>
#include <thread>
#include <vector>

namespace {

lotui::ChildLayout fixedHeight(float height) {
    return {0.0F, {0.0F, height},
        {lotui::unboundedLayoutSize, height}};
}

lotui::TextureImage createDemoMask() {
    constexpr std::uint32_t size = 64;
    lotui::TextureImage image;
    image.width = size;
    image.height = size;
    image.format = lotui::TextureFormat::R8Unorm;
    image.pixels.resize(size * size);
    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            const float dx =
                (static_cast<float>(x) + 0.5F - size * 0.5F) / 27.0F;
            const float dy =
                (static_cast<float>(y) + 0.5F - size * 0.5F) / 27.0F;
            const float coverage = std::clamp(
                (1.0F - std::sqrt(dx * dx + dy * dy)) * 8.0F,
                0.0F,
                1.0F);
            image.pixels[y * size + x] = static_cast<std::uint8_t>(
                coverage * 255.0F);
        }
    }
    return image;
}

std::unique_ptr<lotui::WidgetTree> createDemoUi(
    lotui::TextureId demoMaskTexture,
    const std::shared_ptr<const lotui::TextEngine>& textEngine) {
    auto root = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions rootOptions;
    rootOptions.spacing = 42.0F;
    rootOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    root->setOptions(rootOptions);

    auto leftPanel = std::make_unique<lotui::Column>();
    leftPanel->setDecoration(lotui::BoxDecoration{
        {0.11F, 0.15F, 0.22F, 1.0F}, 18.0F});
    lotui::LinearLayoutOptions leftOptions;
    leftOptions.padding = {34.0F, 38.0F, 34.0F, 40.0F};
    leftOptions.mainAxisAlignment = lotui::MainAxisAlignment::SpaceBetween;
    leftOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    leftPanel->setOptions(leftOptions);
    if (textEngine) {
        auto heading = std::make_unique<lotui::Row>();
        heading->setDecoration(lotui::BoxDecoration{
            {0.18F, 0.23F, 0.34F, 1.0F}, 12.0F});
        lotui::LinearLayoutOptions headingOptions;
        headingOptions.padding = {24.0F, 14.0F, 24.0F, 14.0F};
        headingOptions.crossAxisAlignment =
            lotui::CrossAxisAlignment::Center;
        heading->setOptions(headingOptions);

        lotui::TextStyle headingStyle;
        headingStyle.fontFamilies = {"Noto Sans KR"};
        headingStyle.fontSize = 22.0F;
        headingStyle.weight = lotui::FontWeight::Medium;
        auto headingLabel = std::make_unique<lotui::Label>(
            textEngine, "LotUI 공용 C++ GUI", headingStyle);
        headingLabel->setVerticalAlignment(
            lotui::VerticalTextAlignment::Center);
        heading->addChild(
            std::move(headingLabel),
            {1.0F, {0.0F, 28.0F},
                {lotui::unboundedLayoutSize, 64.0F}});
        leftPanel->addChild(
            std::move(heading),
            {0.0F, {0.0F, 48.0F},
                {lotui::unboundedLayoutSize, 90.0F}});
    } else {
        leftPanel->addChild(
            std::make_unique<lotui::Box>(
                lotui::Size{0.0F, 72.0F},
                lotui::Color{0.18F, 0.23F, 0.34F, 1.0F}, 12.0F),
            {0.0F, {0.0F, 48.0F},
                {lotui::unboundedLayoutSize, 90.0F}});
    }

    auto status = std::make_unique<lotui::Box>(
        lotui::Size{0.0F, 10.0F},
        lotui::Color{0.28F, 0.72F, 0.63F, 1.0F}, 5.0F);
    lotui::Box* statusIndicator = status.get();

    auto buttonRow = std::make_unique<lotui::Row>();
    lotui::LinearLayoutOptions buttonOptions;
    buttonOptions.spacing = 20.0F;
    buttonOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    buttonRow->setOptions(buttonOptions);
    std::unique_ptr<lotui::Widget> primaryContent;
    if (textEngine) {
        lotui::TextStyle buttonTextStyle;
        buttonTextStyle.fontFamilies = {"Noto Sans KR"};
        buttonTextStyle.fontSize = 19.0F;
        auto label = std::make_unique<lotui::Label>(
            textEngine, "확인", buttonTextStyle);
        label->setHorizontalAlignment(lotui::HorizontalTextAlignment::Center);
        label->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
        primaryContent = std::move(label);
    }
    buttonRow->addChild(
        primaryContent
            ? std::make_unique<lotui::Button>(
                std::move(primaryContent),
                lotui::Size{216.0F, 70.0F},
                [statusIndicator]() {
                    statusIndicator->setColor(
                        {0.98F, 0.80F, 0.24F, 1.0F});
                    std::cout << "button-clicked: primary\n";
                })
            : std::make_unique<lotui::Button>(
                lotui::Size{216.0F, 70.0F},
            [statusIndicator]() {
                statusIndicator->setColor(
                    {0.98F, 0.80F, 0.24F, 1.0F});
                std::cout << "button-clicked: primary\n";
            }),
        {1.0F, {80.0F, 48.0F},
            {lotui::unboundedLayoutSize, 90.0F}});

    lotui::ButtonStyle secondaryStyle;
    secondaryStyle.normal = {0.22F, 0.28F, 0.39F, 1.0F};
    secondaryStyle.hovered = {0.30F, 0.37F, 0.50F, 1.0F};
    secondaryStyle.pressed = {0.15F, 0.20F, 0.30F, 1.0F};
    secondaryStyle.cornerRadius = 13.0F;
    std::unique_ptr<lotui::Widget> secondaryContent;
    if (textEngine) {
        lotui::TextStyle buttonTextStyle;
        buttonTextStyle.fontFamilies = {"Noto Sans KR"};
        buttonTextStyle.fontSize = 19.0F;
        auto label = std::make_unique<lotui::Label>(
            textEngine, "취소", buttonTextStyle);
        label->setHorizontalAlignment(lotui::HorizontalTextAlignment::Center);
        label->setVerticalAlignment(lotui::VerticalTextAlignment::Center);
        secondaryContent = std::move(label);
    }
    buttonRow->addChild(
        secondaryContent
            ? std::make_unique<lotui::Button>(
                std::move(secondaryContent),
                lotui::Size{216.0F, 70.0F},
                [statusIndicator]() {
                    statusIndicator->setColor(
                        {0.72F, 0.42F, 0.94F, 1.0F});
                    std::cout << "button-clicked: secondary\n";
                },
                secondaryStyle)
            : std::make_unique<lotui::Button>(
                lotui::Size{216.0F, 70.0F},
            [statusIndicator]() {
                statusIndicator->setColor(
                    {0.72F, 0.42F, 0.94F, 1.0F});
                std::cout << "button-clicked: secondary\n";
            },
            secondaryStyle),
        {1.0F, {80.0F, 48.0F},
            {lotui::unboundedLayoutSize, 90.0F}});
    leftPanel->addChild(
        std::move(buttonRow),
        {0.0F, {0.0F, 48.0F},
            {lotui::unboundedLayoutSize, 90.0F}});
    leftPanel->addChild(std::move(status), fixedHeight(10.0F));

    auto rightPanel = std::make_unique<lotui::Column>();
    rightPanel->setDecoration(lotui::BoxDecoration{
        {0.11F, 0.15F, 0.22F, 1.0F}, 18.0F});
    lotui::LinearLayoutOptions rightOptions;
    rightOptions.padding = {26.0F, 30.0F, 26.0F, 36.0F};
    rightOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    rightPanel->setOptions(rightOptions);

    auto rightContent = std::make_unique<lotui::Column>();
    rightContent->setDecoration(lotui::BoxDecoration{
        {0.18F, 0.23F, 0.34F, 1.0F}, 10.0F});
    lotui::LinearLayoutOptions contentOptions;
    contentOptions.spacing = 24.0F;
    contentOptions.padding = {18.0F, 38.0F, 18.0F, 22.0F};
    contentOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
    rightContent->setOptions(contentOptions);
    auto texturedBox = std::make_unique<lotui::Box>(
        lotui::Size{0.0F, 70.0F},
        lotui::Color{0.62F, 0.38F, 0.92F, 1.0F}, 16.0F);
    texturedBox->setTexture(demoMaskTexture);
    rightContent->addChild(std::move(texturedBox), fixedHeight(70.0F));
    if (textEngine) {
        auto form = std::make_unique<lotui::Column>();
        lotui::LinearLayoutOptions formOptions;
        formOptions.spacing = 12.0F;
        formOptions.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;
        form->setOptions(formOptions);

        lotui::TextStyle controlTextStyle;
        controlTextStyle.fontFamilies = {"Noto Sans KR"};
        controlTextStyle.fontSize = 16.0F;
        form->addChild(
            std::make_unique<lotui::TextField>(
                textEngine,
                "한글 입력",
                lotui::Size{0.0F, 44.0F},
                [](const std::string& text) {
                    std::cout << "text-field-changed: " << text << '\n';
                },
                [](const std::string& text) {
                    std::cout << "text-field-submitted: " << text << '\n';
                },
                lotui::TextFieldStyle{},
                controlTextStyle),
            fixedHeight(44.0F));

        auto checkboxLabel = std::make_unique<lotui::Label>(
            textEngine, "격자에 맞춤", controlTextStyle);
        checkboxLabel->setVerticalAlignment(
            lotui::VerticalTextAlignment::Center);
        form->addChild(
            std::make_unique<lotui::Checkbox>(
                std::move(checkboxLabel),
                true,
                [](bool checked) {
                    std::cout << "checkbox-changed: "
                              << std::boolalpha << checked << '\n';
                }),
            fixedHeight(34.0F));

        lotui::NumericInputOptions numberOptions;
        numberOptions.value = 10.0;
        numberOptions.minimum = 0.5;
        numberOptions.maximum = 100.0;
        numberOptions.step = 0.5;
        numberOptions.decimalPlaces = 1;
        numberOptions.preferredSize = {0.0F, 44.0F};
        form->addChild(
            std::make_unique<lotui::NumericInput>(
                textEngine,
                numberOptions,
                [](double value) {
                    std::cout << "numeric-input-changed: "
                              << value << '\n';
                },
                lotui::NumericInputStyle{},
                controlTextStyle),
            fixedHeight(44.0F));
        rightContent->addChild(
            std::move(form),
            {1.0F, {0.0F, 134.0F},
                {lotui::unboundedLayoutSize, 160.0F}});
    } else {
        rightContent->addChild(
            std::make_unique<lotui::Box>(
                lotui::Size{0.0F, 86.0F},
                lotui::Color{0.96F, 0.55F, 0.24F, 1.0F}, 16.0F),
            {1.0F, {0.0F, 50.0F},
                {lotui::unboundedLayoutSize, 132.0F}});
    }
    rightPanel->addChild(
        std::move(rightContent),
        {1.0F, {0.0F, 120.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});

    root->addChild(
        std::move(leftPanel),
        {1.7F, {260.0F, 180.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    root->addChild(
        std::move(rightPanel),
        {1.0F, {180.0F, 180.0F},
            {lotui::unboundedLayoutSize, lotui::unboundedLayoutSize}});
    return std::make_unique<lotui::WidgetTree>(std::move(root));
}

void updateLayout(
    lotui::WidgetTree& tree,
    const lotui::WindowMetrics& metrics) {
    const float scale = metrics.dpiScale > 0.0F ? metrics.dpiScale : 1.0F;
    const float width =
        static_cast<float>(metrics.framebufferWidth) / scale;
    const float height =
        static_cast<float>(metrics.framebufferHeight) / scale;
    tree.layout(
        {48.0F, 44.0F, std::max(0.0F, width - 96.0F),
            std::min(400.0F, std::max(0.0F, height - 88.0F))},
        {0.0F, 0.0F, width, height});
}

void applyPointerUpdate(
    const lotui::WidgetPointerUpdate& update,
    lotui::WidgetTree& tree,
    lotui::PlatformWindow& window) {
    if (update.captureStarted && !window.setPointerCapture(true)) {
        tree.cancelPointer();
        std::cerr << "pointer-capture failed\n";
    }
    if (update.captureEnded) {
        window.setPointerCapture(false);
    }
}

void printEvent(const lotui::PlatformEvent& event) {
    std::cout << "event: " << lotui::eventTypeName(event.type);
    switch (event.type) {
    case lotui::PlatformEventType::Resized:
        std::cout << " width=" << event.width << " height=" << event.height;
        break;
    case lotui::PlatformEventType::MouseMoved:
        std::cout << " x=" << event.x << " y=" << event.y;
        break;
    case lotui::PlatformEventType::MouseButtonPressed:
    case lotui::PlatformEventType::MouseButtonReleased:
        std::cout << " x=" << event.x << " y=" << event.y
                  << " button=" << lotui::pointerButtonName(event.button);
        break;
    case lotui::PlatformEventType::KeyPressed:
    case lotui::PlatformEventType::KeyReleased:
        std::cout << " key=" << lotui::keyCodeName(event.key)
                  << " shift=" << event.modifiers.shift
                  << " control=" << event.modifiers.control
                  << " alt=" << event.modifiers.alt
                  << " meta=" << event.modifiers.meta
                  << " repeat=" << std::boolalpha << event.repeat;
        break;
    case lotui::PlatformEventType::DpiChanged:
        std::cout << " scale=" << std::fixed << std::setprecision(2)
                  << event.dpiScale;
        break;
    case lotui::PlatformEventType::TextInput:
    case lotui::PlatformEventType::TextComposition:
        std::cout << " text=" << event.text
                  << " selection=" << event.selectionStart
                  << '+' << event.selectionLength;
        break;
    case lotui::PlatformEventType::CloseRequested:
    case lotui::PlatformEventType::PointerCaptureLost:
    case lotui::PlatformEventType::TextCompositionEnd:
    case lotui::PlatformEventType::FocusGained:
    case lotui::PlatformEventType::FocusLost:
        break;
    }
    std::cout << '\n';
}

} // namespace

int main() {
    try {
        auto platform = lotui::createPlatformBackend();
        auto window = platform->createWindow(
            {"LotUI Platform Probe", 960, 640, true});
        window->show();

        lotui::VulkanRenderer renderer(*window);
        lotui::Texture demoMask = renderer.createTexture(createDemoMask());
        std::shared_ptr<const lotui::TextEngine> textEngine;
#ifdef LOTUI_HAS_EXAMPLE_TEXT
        const auto fontFile = lotui::executableDirectory() /
            "resources" / "fonts" / "NotoSansKR-Regular.ttf";
        textEngine = std::make_shared<lotui::FreetypeTextEngine>(
            renderer,
            std::vector<lotui::FontSource>{
                {"Noto Sans KR", fontFile}});
        std::cout << "font-loaded: " << fontFile.string() << '\n';
#endif
        auto tree = createDemoUi(demoMask.id(), textEngine);
        updateLayout(*tree, window->metrics());
        window->setTextInputState(tree->textInputState());

        const auto initial = window->metrics();
        std::cout << "window-created: width=" << initial.width
                  << " height=" << initial.height
                  << " framebuffer=" << initial.framebufferWidth << 'x'
                  << initial.framebufferHeight
                  << " dpi-scale=" << std::fixed << std::setprecision(2)
                  << initial.dpiScale << '\n';

        std::vector<lotui::PaintCommand> commands;
        bool running = true;
        while (running) {
            bool receivedEvent = false;
            lotui::PlatformEvent event{};
            while (window->pollEvent(event)) {
                receivedEvent = true;
                printEvent(event);
                if (event.type == lotui::PlatformEventType::CloseRequested) {
                    running = false;
                } else if (
                    event.type == lotui::PlatformEventType::Resized ||
                    event.type == lotui::PlatformEventType::DpiChanged) {
                    updateLayout(*tree, window->metrics());
                }

                if (event.type == lotui::PlatformEventType::KeyPressed) {
                    tree->keyPressed(
                        event.key, event.modifiers, event.repeat);
                } else if (
                    event.type == lotui::PlatformEventType::KeyReleased) {
                    tree->keyReleased(event.key, event.modifiers);
                } else if (
                    event.type == lotui::PlatformEventType::TextInput) {
                    tree->textInput({
                        lotui::TextInputEventType::Commit,
                        event.text,
                        event.selectionStart,
                        event.selectionLength});
                } else if (
                    event.type == lotui::PlatformEventType::TextComposition) {
                    tree->textInput({
                        lotui::TextInputEventType::Composition,
                        event.text,
                        event.selectionStart,
                        event.selectionLength});
                } else if (
                    event.type ==
                        lotui::PlatformEventType::TextCompositionEnd) {
                    tree->textInput({
                        lotui::TextInputEventType::CompositionEnd});
                } else if (
                    event.type == lotui::PlatformEventType::FocusLost) {
                    tree->cancelKeyboard();
                }

                const lotui::Point position{event.x, event.y};
                lotui::WidgetPointerUpdate update;
                if (event.type == lotui::PlatformEventType::MouseMoved) {
                    update = tree->pointerMoved(position);
                } else if (
                    event.type == lotui::PlatformEventType::MouseButtonPressed &&
                    event.button == lotui::PointerButton::Primary) {
                    update = tree->pointerPressed(position, event.button);
                } else if (
                    event.type == lotui::PlatformEventType::MouseButtonReleased &&
                    event.button == lotui::PointerButton::Primary) {
                    update = tree->pointerReleased(position, event.button);
                } else if (
                    event.type == lotui::PlatformEventType::PointerCaptureLost ||
                    event.type == lotui::PlatformEventType::FocusLost) {
                    update = tree->cancelPointer();
                }
                applyPointerUpdate(update, *tree, *window);
                window->setTextInputState(
                    event.type == lotui::PlatformEventType::FocusLost
                        ? lotui::TextInputState{}
                        : tree->textInputState());
            }

            if (running) {
                commands.clear();
                tree->paint(commands);
                window->setTextInputState(tree->textInputState());
                renderer.drawFrame(commands);
            }
            if (!receivedEvent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "platform_probe failed: " << error.what() << '\n';
        return 1;
    }
}
