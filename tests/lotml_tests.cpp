#include "declarative/lotml.h"

#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/numeric_input.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "lotml_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class TestTextLayout final : public lotui::TextLayout {
public:
    explicit TestTextLayout(lotui::Size size)
        : size_(size) {
    }

    lotui::Size size() const noexcept override {
        return size_;
    }

    float baseline() const noexcept override {
        return size_.height * 0.8F;
    }

    void appendPaintCommands(
        lotui::Point,
        lotui::Rect,
        lotui::Color,
        std::vector<lotui::PaintCommand>&) const override {
    }

private:
    lotui::Size size_{};
};

class TestTextEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view text,
        const lotui::TextStyle& style,
        const lotui::TextLayoutOptions& options) const override {
        const float naturalWidth =
            static_cast<float>(text.size()) * style.fontSize * 0.5F;
        const float width = std::isfinite(options.maximumWidth)
            ? std::min(naturalWidth, options.maximumWidth)
            : naturalWidth;
        return std::make_unique<TestTextLayout>(
            lotui::Size{width, style.fontSize * 1.2F});
    }
};

template<typename Action>
void requireLotmlError(Action action, const char* message) {
    try {
        action();
    } catch (const lotui::declarative::LotmlError&) {
        return;
    }
    require(false, message);
}

void loadsLayoutAndDispatchesNamedEvents() {
    constexpr std::string_view source = R"lotml(
<Column id="root" spacing="12" padding="16" background="#172033">
  <Label id="title" text="LotUI 설정" fontSize="20" />
  <Row id="actions" spacing="8" flex="1" crossAlign="stretch">
    <Button id="save" text="저장" onClick="save" flex="1" />
    <Button id="cancel" text="취소" onClick="cancel" />
  </Row>
</Column>)lotml";

    int saveCount = 0;
    int cancelCount = 0;
    lotui::declarative::LoadOptions options;
    options.textEngine = std::make_shared<TestTextEngine>();
    options.events.emplace("save", [&saveCount]() { ++saveCount; });
    options.events.emplace("cancel", [&cancelCount]() { ++cancelCount; });

    lotui::declarative::LotmlLoader loader;
    auto loaded = loader.loadString(source, std::move(options));

    require(dynamic_cast<lotui::Column*>(loaded.find("root")) != nullptr,
        "root id must resolve to the Column");
    require(dynamic_cast<lotui::Label*>(loaded.find("title")) != nullptr,
        "label id must resolve to the Label");
    require(dynamic_cast<lotui::Button*>(loaded.find("save")) != nullptr,
        "button id must resolve to the Button");
    require(loaded.find("missing") == nullptr,
        "unknown ids must return nullptr");

    loaded.tree().layout({0.0F, 0.0F, 640.0F, 360.0F});
    const auto focus = loaded.tree().keyPressed(lotui::KeyCode::Tab);
    require(focus.handled && focus.focusChanged,
        "Tab must focus the first declarative button");
    loaded.tree().keyPressed(lotui::KeyCode::Enter);
    loaded.tree().keyReleased(lotui::KeyCode::Enter);
    require(saveCount == 1 && cancelCount == 0,
        "named onClick event must run through keyboard activation");
}

void exposesMetadataForFutureDesignTools() {
    lotui::declarative::LotmlLoader loader;
    const auto* button = loader.registry().find("Button");
    require(button != nullptr && button->allowsChildren,
        "Button metadata must be discoverable");

    bool foundEvent = false;
    for (const auto& property : button->properties) {
        if (property.name == "onClick" &&
            property.type == lotui::declarative::PropertyType::Event) {
            foundEvent = true;
        }
    }
    require(foundEvent,
        "design tools must be able to identify event properties");
}

void loadsFormControlsThroughTheSharedWidgetTree() {
    constexpr std::string_view source = R"lotml(
<Column spacing="8">
  <Checkbox id="snap" text="격자에 맞춤" checked="true"
            onChanged="formChanged" />
  <NumericInput id="gridSize" value="10" minimum="1" maximum="20"
                step="0.5" decimalPlaces="1" onChanged="formChanged" />
</Column>)lotml";

    int changes = 0;
    lotui::declarative::LoadOptions options;
    options.textEngine = std::make_shared<TestTextEngine>();
    options.events.emplace(
        "formChanged", [&changes]() { ++changes; });

    lotui::declarative::LotmlLoader loader;
    auto loaded = loader.loadString(source, std::move(options));
    auto* checkbox = dynamic_cast<lotui::Checkbox*>(loaded.find("snap"));
    auto* number = dynamic_cast<lotui::NumericInput*>(
        loaded.find("gridSize"));
    require(checkbox != nullptr && checkbox->isChecked(),
        "LotML must construct the configured Checkbox");
    require(number != nullptr && std::abs(number->value() - 10.0) < 0.001,
        "LotML must construct the configured NumericInput");

    loaded.tree().layout({0.0F, 0.0F, 320.0F, 120.0F});
    loaded.tree().keyPressed(lotui::KeyCode::Tab);
    loaded.tree().keyPressed(lotui::KeyCode::Space);
    loaded.tree().keyReleased(lotui::KeyCode::Space);
    loaded.tree().keyPressed(lotui::KeyCode::Tab);
    loaded.tree().keyPressed(lotui::KeyCode::Up);
    require(!checkbox->isChecked() &&
            std::abs(number->value() - 10.5) < 0.001 && changes == 2,
        "declarative form controls must share focus and event dispatch");
}

void rejectsInvalidDocuments() {
    lotui::declarative::LotmlLoader loader;
    const auto engine = std::make_shared<TestTextEngine>();

    requireLotmlError(
        [&]() { loader.loadString("<Column><Button></Column>"); },
        "malformed XML must fail");
    requireLotmlError(
        [&]() {
            lotui::declarative::LoadOptions options;
            options.textEngine = engine;
            loader.loadString("<Label text=\"x\" mystery=\"1\" />",
                std::move(options));
        },
        "unknown properties must fail");
    requireLotmlError(
        [&]() {
            lotui::declarative::LoadOptions options;
            options.textEngine = engine;
            loader.loadString(
                "<Row><Label id=\"same\" text=\"a\" />"
                "<Label id=\"same\" text=\"b\" /></Row>",
                std::move(options));
        },
        "duplicate ids must fail");
    requireLotmlError(
        [&]() {
            lotui::declarative::LoadOptions options;
            options.textEngine = engine;
            loader.loadString(
                "<Button text=\"x\" onClick=\"missing\" />",
                std::move(options));
        },
        "unregistered events must fail");
    requireLotmlError(
        [&]() { loader.loadString("<Label text=\"x\" />"); },
        "text widgets must require a text engine");
    requireLotmlError(
        [&]() { loader.loadString("<Box>unexpected</Box>"); },
        "non-text widgets must reject text content");
}

} // namespace

int main() {
    loadsLayoutAndDispatchesNamedEvents();
    exposesMetadataForFutureDesignTools();
    loadsFormControlsThroughTheSharedWidgetTree();
    rejectsInvalidDocuments();
    std::cout << "lotml_tests passed\n";
    return EXIT_SUCCESS;
}
