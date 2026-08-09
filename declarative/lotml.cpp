#include "declarative/lotml.h"

#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/numeric_input.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace lotui::declarative {
namespace {

std::string trim(std::string value) {
    const auto whitespace = [](unsigned char character) {
        return character == ' ' || character == '\t' ||
            character == '\r' || character == '\n';
    };
    const auto first = std::find_if_not(
        value.begin(), value.end(), whitespace);
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(), whitespace).base();
    return first < last ? std::string(first, last) : std::string{};
}

[[noreturn]] void fail(
    const LotmlElement& element,
    const std::string& message) {
    std::ostringstream stream;
    stream << "LotML";
    if (element.line > 0) {
        stream << " line " << element.line;
    }
    stream << ": " << message;
    throw LotmlError(stream.str());
}

const std::string* attribute(
    const LotmlElement& element,
    std::string_view name) noexcept {
    return element.findAttribute(name);
}

float parseNumberText(
    const LotmlElement& element,
    std::string_view name,
    const std::string& text) {
    char* end = nullptr;
    errno = 0;
    const float value = std::strtof(text.c_str(), &end);
    if (errno == ERANGE || end == text.c_str() ||
        end != text.c_str() + text.size() || !std::isfinite(value)) {
        fail(element, "attribute '" + std::string(name) +
            "' must be a finite number");
    }
    return value;
}

float number(
    const LotmlElement& element,
    std::string_view name,
    float fallback) {
    const std::string* value = attribute(element, name);
    return value == nullptr
        ? fallback
        : parseNumberText(element, name, *value);
}

bool boolean(
    const LotmlElement& element,
    std::string_view name,
    bool fallback) {
    const std::string* value = attribute(element, name);
    if (value == nullptr) {
        return fallback;
    }
    if (*value == "true" || *value == "1") {
        return true;
    }
    if (*value == "false" || *value == "0") {
        return false;
    }
    fail(element, "attribute '" + std::string(name) +
        "' must be true or false");
}

unsigned int hexDigit(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<unsigned int>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<unsigned int>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<unsigned int>(value - 'A' + 10);
    }
    return 16;
}

Color colorValue(
    const LotmlElement& element,
    std::string_view name,
    Color fallback) {
    const std::string* value = attribute(element, name);
    if (value == nullptr) {
        return fallback;
    }
    if ((value->size() != 7 && value->size() != 9) || (*value)[0] != '#') {
        fail(element, "attribute '" + std::string(name) +
            "' must use #RRGGBB or #RRGGBBAA");
    }
    const auto byte = [&](std::size_t offset) {
        const unsigned int high = hexDigit((*value)[offset]);
        const unsigned int low = hexDigit((*value)[offset + 1]);
        if (high > 15 || low > 15) {
            fail(element, "attribute '" + std::string(name) +
                "' contains an invalid hexadecimal color");
        }
        return static_cast<float>((high << 4U) | low) / 255.0F;
    };
    return {
        byte(1), byte(3), byte(5),
        value->size() == 9 ? byte(7) : 1.0F,
    };
}

EdgeInsets insets(
    const LotmlElement& element,
    std::string_view name,
    EdgeInsets fallback = {}) {
    const std::string* value = attribute(element, name);
    if (value == nullptr) {
        return fallback;
    }
    std::vector<float> parts;
    std::size_t begin = 0;
    while (begin <= value->size()) {
        const std::size_t comma = value->find(',', begin);
        const std::size_t end = comma == std::string::npos
            ? value->size()
            : comma;
        const std::string part = trim(value->substr(begin, end - begin));
        if (part.empty()) {
            fail(element, "attribute '" + std::string(name) +
                "' contains an empty inset");
        }
        parts.push_back(parseNumberText(element, name, part));
        if (comma == std::string::npos) {
            break;
        }
        begin = comma + 1;
    }
    if (parts.size() == 1) {
        return EdgeInsets::all(parts[0]);
    }
    if (parts.size() == 2) {
        return {parts[0], parts[1], parts[0], parts[1]};
    }
    if (parts.size() == 4) {
        return {parts[0], parts[1], parts[2], parts[3]};
    }
    fail(element, "attribute '" + std::string(name) +
        "' requires one, two, or four values");
}

std::string stringValue(
    const LotmlElement& element,
    std::string_view name,
    std::string fallback = {}) {
    const std::string* value = attribute(element, name);
    return value == nullptr ? std::move(fallback) : *value;
}

MainAxisAlignment mainAlignment(const LotmlElement& element) {
    const std::string value = stringValue(element, "mainAlign", "start");
    if (value == "start") return MainAxisAlignment::Start;
    if (value == "center") return MainAxisAlignment::Center;
    if (value == "end") return MainAxisAlignment::End;
    if (value == "spaceBetween") return MainAxisAlignment::SpaceBetween;
    fail(element, "mainAlign must be start, center, end, or spaceBetween");
}

CrossAxisAlignment crossAlignment(const LotmlElement& element) {
    const std::string value = stringValue(element, "crossAlign", "start");
    if (value == "start") return CrossAxisAlignment::Start;
    if (value == "center") return CrossAxisAlignment::Center;
    if (value == "end") return CrossAxisAlignment::End;
    if (value == "stretch") return CrossAxisAlignment::Stretch;
    fail(element, "crossAlign must be start, center, end, or stretch");
}

HorizontalTextAlignment horizontalTextAlignment(
    const LotmlElement& element) {
    const std::string value = stringValue(element, "horizontalAlign", "start");
    if (value == "start") return HorizontalTextAlignment::Start;
    if (value == "center") return HorizontalTextAlignment::Center;
    if (value == "end") return HorizontalTextAlignment::End;
    fail(element, "horizontalAlign must be start, center, or end");
}

VerticalTextAlignment verticalTextAlignment(const LotmlElement& element) {
    const std::string value = stringValue(element, "verticalAlign", "top");
    if (value == "top") return VerticalTextAlignment::Top;
    if (value == "center") return VerticalTextAlignment::Center;
    if (value == "bottom") return VerticalTextAlignment::Bottom;
    fail(element, "verticalAlign must be top, center, or bottom");
}

ChildLayout childLayout(const LotmlElement& element) {
    ChildLayout result;
    result.flex = std::max(0.0F, number(element, "flex", 0.0F));
    result.minimum = {
        std::max(0.0F, number(element, "minWidth", 0.0F)),
        std::max(0.0F, number(element, "minHeight", 0.0F)),
    };
    result.maximum = {
        std::max(result.minimum.width,
            number(element, "maxWidth", unboundedLayoutSize)),
        std::max(result.minimum.height,
            number(element, "maxHeight", unboundedLayoutSize)),
    };
    return result;
}

LotmlElement convertElement(const tinyxml2::XMLElement& source) {
    LotmlElement result;
    result.type = source.Name() == nullptr ? "" : source.Name();
    result.line = source.GetLineNum();
    for (const tinyxml2::XMLAttribute* attribute = source.FirstAttribute();
         attribute != nullptr;
         attribute = attribute->Next()) {
        result.attributes.emplace(attribute->Name(), attribute->Value());
    }
    for (const tinyxml2::XMLNode* node = source.FirstChild();
         node != nullptr;
         node = node->NextSibling()) {
        if (const auto* child = node->ToElement()) {
            result.children.push_back(convertElement(*child));
        } else if (const auto* textNode = node->ToText()) {
            result.text += textNode->Value();
        } else if (node->ToComment() == nullptr) {
            fail(result, "unsupported XML node inside <" + result.type + ">");
        }
    }
    result.text = trim(std::move(result.text));
    return result;
}

LotmlElement parseDocument(std::string_view xml) {
    tinyxml2::XMLDocument document;
    const tinyxml2::XMLError error = document.Parse(xml.data(), xml.size());
    if (error != tinyxml2::XML_SUCCESS) {
        std::ostringstream stream;
        stream << "LotML parse error";
        if (document.ErrorLineNum() > 0) {
            stream << " at line " << document.ErrorLineNum();
        }
        stream << ": " << document.ErrorStr();
        throw LotmlError(stream.str());
    }
    const tinyxml2::XMLElement* root = document.RootElement();
    if (root == nullptr) {
        throw LotmlError("LotML document requires a root widget");
    }
    if (root->NextSiblingElement() != nullptr) {
        throw LotmlError("LotML document requires exactly one root widget");
    }
    return convertElement(*root);
}

std::vector<PropertyDescriptor> linearProperties() {
    return {
        {"spacing", PropertyType::Number, "0"},
        {"padding", PropertyType::Insets, "0"},
        {"mainAlign", PropertyType::Enumeration, "start"},
        {"crossAlign", PropertyType::Enumeration, "start"},
        {"background", PropertyType::Color, "#00000000"},
        {"cornerRadius", PropertyType::Number, "0"},
    };
}

std::unique_ptr<Widget> buildLinear(
    const LotmlElement& element,
    BuildContext& context,
    bool horizontal) {
    std::unique_ptr<LinearLayout> layout;
    if (horizontal) {
        layout = std::make_unique<Row>();
    } else {
        layout = std::make_unique<Column>();
    }
    LinearLayoutOptions options;
    options.spacing = std::max(0.0F, number(element, "spacing", 0.0F));
    options.padding = insets(element, "padding");
    options.mainAxisAlignment = mainAlignment(element);
    options.crossAxisAlignment = crossAlignment(element);
    layout->setOptions(options);
    if (attribute(element, "background") != nullptr) {
        layout->setDecoration(BoxDecoration{
            colorValue(element, "background", {}),
            std::max(0.0F, number(element, "cornerRadius", 0.0F)),
        });
    }
    for (const LotmlElement& child : element.children) {
        layout->addChild(context.buildChild(child), childLayout(child));
    }
    return layout;
}

std::unique_ptr<Widget> buildLabel(
    const LotmlElement& element,
    BuildContext& context) {
    const auto engine = context.textEngine();
    if (!engine) {
        fail(element, "Label requires LoadOptions.textEngine");
    }
    TextStyle style;
    style.fontFamilies = {
        stringValue(element, "fontFamily", "sans-serif")};
    style.fontSize = std::max(1.0F, number(element, "fontSize", 14.0F));
    style.letterSpacing = number(element, "letterSpacing", 0.0F);
    style.lineHeight = std::max(0.0F, number(element, "lineHeight", 0.0F));
    style.italic = boolean(element, "italic", false);
    const float weight = number(element, "fontWeight", 400.0F);
    const int roundedWeight = static_cast<int>(std::round(weight / 100.0F)) * 100;
    if (roundedWeight < 100 || roundedWeight > 900 ||
        std::abs(weight - roundedWeight) > 0.01F) {
        fail(element, "fontWeight must be 100 through 900 in steps of 100");
    }
    style.weight = static_cast<FontWeight>(roundedWeight);

    std::string text = stringValue(element, "text", element.text);
    auto label = std::make_unique<Label>(engine, std::move(text), style);
    label->setColor(colorValue(
        element, "color", {0.93F, 0.95F, 1.0F, 1.0F}));
    label->setHorizontalAlignment(horizontalTextAlignment(element));
    label->setVerticalAlignment(verticalTextAlignment(element));
    return label;
}

std::unique_ptr<Widget> buildBox(
    const LotmlElement& element,
    BuildContext&) {
    return std::make_unique<Box>(
        Size{
            std::max(0.0F, number(element, "width", 0.0F)),
            std::max(0.0F, number(element, "height", 0.0F)),
        },
        colorValue(element, "color", {}),
        std::max(0.0F, number(element, "cornerRadius", 0.0F)));
}

std::unique_ptr<Widget> buildButton(
    const LotmlElement& element,
    BuildContext& context) {
    if (element.children.size() > 1) {
        fail(element, "Button accepts at most one child");
    }
    const std::string text = stringValue(element, "text", element.text);
    if (!element.children.empty() && !text.empty()) {
        fail(element, "Button cannot use both text and a child widget");
    }

    std::unique_ptr<Widget> content;
    if (!element.children.empty()) {
        content = context.buildChild(element.children.front());
    } else if (!text.empty()) {
        const auto engine = context.textEngine();
        if (!engine) {
            fail(element, "text Button requires LoadOptions.textEngine");
        }
        TextStyle textStyle;
        textStyle.fontFamilies = {
            stringValue(element, "fontFamily", "sans-serif")};
        textStyle.fontSize = std::max(
            1.0F, number(element, "fontSize", 14.0F));
        auto label = std::make_unique<Label>(engine, text, textStyle);
        label->setHorizontalAlignment(HorizontalTextAlignment::Center);
        label->setVerticalAlignment(VerticalTextAlignment::Center);
        content = std::move(label);
    }

    ButtonStyle style;
    style.normal = colorValue(element, "normal", style.normal);
    style.hovered = colorValue(element, "hovered", style.hovered);
    style.pressed = colorValue(element, "pressed", style.pressed);
    style.disabled = colorValue(element, "disabled", style.disabled);
    style.focusRing = colorValue(element, "focusRing", style.focusRing);
    style.cornerRadius = std::max(
        0.0F, number(element, "cornerRadius", style.cornerRadius));
    style.focusRingWidth = std::max(
        0.0F, number(element, "focusRingWidth", style.focusRingWidth));
    style.contentPadding = insets(
        element, "contentPadding", style.contentPadding);

    std::function<void()> callback;
    if (const std::string* eventName = attribute(element, "onClick")) {
        callback = context.event(*eventName);
    }
    const Size minimum{
        std::max(0.0F, number(element, "width", 120.0F)),
        std::max(0.0F, number(element, "height", 40.0F)),
    };
    if (content) {
        return std::make_unique<Button>(
            std::move(content), minimum, std::move(callback), style);
    }
    return std::make_unique<Button>(minimum, std::move(callback), style);
}

std::unique_ptr<Widget> buildCheckbox(
    const LotmlElement& element,
    BuildContext& context) {
    if (element.children.size() > 1) {
        fail(element, "Checkbox accepts at most one child");
    }
    const std::string text = stringValue(element, "text", element.text);
    if (!element.children.empty() && !text.empty()) {
        fail(element, "Checkbox cannot use both text and a child widget");
    }

    std::unique_ptr<Widget> content;
    if (!element.children.empty()) {
        content = context.buildChild(element.children.front());
    } else if (!text.empty()) {
        const auto engine = context.textEngine();
        if (!engine) {
            fail(element, "text Checkbox requires LoadOptions.textEngine");
        }
        TextStyle textStyle;
        textStyle.fontFamilies = {
            stringValue(element, "fontFamily", "sans-serif")};
        textStyle.fontSize = std::max(
            1.0F, number(element, "fontSize", 14.0F));
        auto label = std::make_unique<Label>(engine, text, textStyle);
        label->setColor(colorValue(
            element, "textColor", {0.93F, 0.95F, 1.0F, 1.0F}));
        label->setVerticalAlignment(VerticalTextAlignment::Center);
        content = std::move(label);
    }

    CheckboxStyle style;
    style.unchecked = colorValue(element, "unchecked", style.unchecked);
    style.hovered = colorValue(element, "hovered", style.hovered);
    style.checked = colorValue(element, "checkedColor", style.checked);
    style.checkmark = colorValue(element, "checkmark", style.checkmark);
    style.disabled = colorValue(element, "disabled", style.disabled);
    style.focusRing = colorValue(element, "focusRing", style.focusRing);
    style.boxSize = std::max(
        0.0F, number(element, "boxSize", style.boxSize));
    style.spacing = std::max(
        0.0F, number(element, "spacing", style.spacing));
    style.cornerRadius = std::max(
        0.0F, number(element, "cornerRadius", style.cornerRadius));
    style.focusRingWidth = std::max(
        0.0F, number(element, "focusRingWidth", style.focusRingWidth));

    Checkbox::ChangedHandler callback;
    if (const std::string* eventName = attribute(element, "onChanged")) {
        auto event = context.event(*eventName);
        callback = [event = std::move(event)](bool) { event(); };
    }
    auto checkbox = content
        ? std::make_unique<Checkbox>(
            std::move(content),
            boolean(element, "checked", false),
            std::move(callback),
            style)
        : std::make_unique<Checkbox>(
            boolean(element, "checked", false),
            std::move(callback),
            style);
    checkbox->setEnabled(boolean(element, "enabled", true));
    return checkbox;
}

std::unique_ptr<Widget> buildNumericInput(
    const LotmlElement& element,
    BuildContext& context) {
    const auto engine = context.textEngine();
    if (!engine) {
        fail(element, "NumericInput requires LoadOptions.textEngine");
    }

    NumericInputOptions options;
    options.value = number(element, "value", 0.0F);
    options.minimum = number(element, "minimum", 0.0F);
    options.maximum = number(element, "maximum", 100.0F);
    options.step = number(element, "step", 1.0F);
    const float decimals = number(element, "decimalPlaces", 0.0F);
    const int roundedDecimals = static_cast<int>(std::round(decimals));
    if (options.minimum > options.maximum) {
        fail(element, "minimum must not exceed maximum");
    }
    if (options.step <= 0.0) {
        fail(element, "step must be greater than zero");
    }
    if (roundedDecimals < 0 || roundedDecimals > 9 ||
        std::abs(decimals - static_cast<float>(roundedDecimals)) > 0.01F) {
        fail(element, "decimalPlaces must be an integer from 0 through 9");
    }
    options.decimalPlaces = roundedDecimals;
    options.preferredSize = {
        std::max(0.0F, number(element, "width", 160.0F)),
        std::max(0.0F, number(element, "height", 40.0F)),
    };

    NumericInputStyle style;
    style.normal = colorValue(element, "normal", style.normal);
    style.hovered = colorValue(element, "hovered", style.hovered);
    style.stepButton = colorValue(
        element, "stepButton", style.stepButton);
    style.stepButtonPressed = colorValue(
        element, "stepButtonPressed", style.stepButtonPressed);
    style.indicator = colorValue(element, "indicator", style.indicator);
    style.text = colorValue(element, "textColor", style.text);
    style.disabled = colorValue(element, "disabled", style.disabled);
    style.focusRing = colorValue(element, "focusRing", style.focusRing);
    style.contentPadding = insets(
        element, "contentPadding", style.contentPadding);
    style.stepButtonWidth = std::max(
        0.0F, number(element, "stepButtonWidth", style.stepButtonWidth));
    style.cornerRadius = std::max(
        0.0F, number(element, "cornerRadius", style.cornerRadius));
    style.focusRingWidth = std::max(
        0.0F, number(element, "focusRingWidth", style.focusRingWidth));

    TextStyle textStyle;
    textStyle.fontFamilies = {
        stringValue(element, "fontFamily", "sans-serif")};
    textStyle.fontSize = std::max(
        1.0F, number(element, "fontSize", 14.0F));

    NumericInput::ChangedHandler callback;
    if (const std::string* eventName = attribute(element, "onChanged")) {
        auto event = context.event(*eventName);
        callback = [event = std::move(event)](double) { event(); };
    }
    auto input = std::make_unique<NumericInput>(
        engine, options, std::move(callback), style, textStyle);
    input->setEnabled(boolean(element, "enabled", true));
    return input;
}

const std::unordered_set<std::string>& layoutAttributeNames() {
    static const std::unordered_set<std::string> names{
        "id", "flex", "minWidth", "minHeight", "maxWidth", "maxHeight"};
    return names;
}

} // namespace

const std::string* LotmlElement::findAttribute(
    std::string_view name) const noexcept {
    const auto found = attributes.find(std::string(name));
    return found == attributes.end() ? nullptr : &found->second;
}

void WidgetRegistry::registerWidget(
    WidgetDescriptor descriptor,
    WidgetFactory factory) {
    if (descriptor.type.empty() || !factory) {
        throw std::invalid_argument(
            "widget registration requires a type and factory");
    }
    const std::string type = descriptor.type;
    const auto inserted = entries_.emplace(
        type, Entry{std::move(descriptor), std::move(factory)});
    if (!inserted.second) {
        throw std::invalid_argument(
            "widget type is already registered: " + type);
    }
}

const WidgetDescriptor* WidgetRegistry::find(
    std::string_view type) const noexcept {
    const auto found = entries_.find(std::string(type));
    return found == entries_.end() ? nullptr : &found->second.descriptor;
}

std::vector<WidgetDescriptor> WidgetRegistry::descriptors() const {
    std::vector<WidgetDescriptor> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_) {
        result.push_back(entry.second.descriptor);
    }
    std::sort(
        result.begin(), result.end(),
        [](const WidgetDescriptor& left, const WidgetDescriptor& right) {
            return left.type < right.type;
        });
    return result;
}

struct BuildContext::State {
    const WidgetRegistry* registry{nullptr};
    LoadOptions options;
    std::unordered_map<std::string, Widget*> widgetsById;
};

BuildContext::BuildContext(State& state) noexcept
    : state_(&state) {
}

std::unique_ptr<Widget> BuildContext::buildChild(
    const LotmlElement& element) {
    const auto found = state_->registry->entries_.find(element.type);
    if (found == state_->registry->entries_.end()) {
        fail(element, "unknown widget type <" + element.type + ">");
    }
    const WidgetRegistry::Entry& entry = found->second;
    if (!entry.descriptor.allowsChildren && !element.children.empty()) {
        fail(element, "<" + element.type + "> does not accept child widgets");
    }
    if (!element.text.empty() && element.type != "Label" &&
        element.type != "Button" && element.type != "Checkbox") {
        fail(element, "<" + element.type +
            "> does not accept text content");
    }

    std::unordered_set<std::string> properties;
    for (const PropertyDescriptor& property : entry.descriptor.properties) {
        properties.insert(property.name);
        if (property.required && attribute(element, property.name) == nullptr) {
            fail(element, "missing required attribute '" + property.name + "'");
        }
    }
    for (const auto& value : element.attributes) {
        if (properties.find(value.first) == properties.end() &&
            layoutAttributeNames().find(value.first) ==
                layoutAttributeNames().end()) {
            fail(element, "unknown attribute '" + value.first +
                "' on <" + element.type + ">");
        }
    }

    std::unique_ptr<Widget> widget = entry.factory(element, *this);
    if (!widget) {
        fail(element, "factory returned no widget for <" + element.type + ">");
    }
    if (const std::string* id = attribute(element, "id")) {
        if (id->empty()) {
            fail(element, "id must not be empty");
        }
        if (!state_->widgetsById.emplace(*id, widget.get()).second) {
            fail(element, "duplicate id '" + *id + "'");
        }
    }
    return widget;
}

std::shared_ptr<const TextEngine> BuildContext::textEngine() const noexcept {
    return state_->options.textEngine;
}

std::function<void()> BuildContext::event(std::string_view name) const {
    const auto found = state_->options.events.find(std::string(name));
    if (found == state_->options.events.end()) {
        throw LotmlError("LotML event is not registered: " +
            std::string(name));
    }
    return found->second;
}

LoadedUi::LoadedUi(
    std::unique_ptr<WidgetTree> tree,
    std::unordered_map<std::string, Widget*> widgetsById)
    : tree_(std::move(tree)),
      widgetsById_(std::move(widgetsById)) {
}

WidgetTree& LoadedUi::tree() noexcept {
    return *tree_;
}

const WidgetTree& LoadedUi::tree() const noexcept {
    return *tree_;
}

Widget* LoadedUi::find(std::string_view id) noexcept {
    const auto found = widgetsById_.find(std::string(id));
    return found == widgetsById_.end() ? nullptr : found->second;
}

const Widget* LoadedUi::find(std::string_view id) const noexcept {
    return const_cast<LoadedUi*>(this)->find(id);
}

LotmlLoader::LotmlLoader()
    : registry_(createDefaultWidgetRegistry()) {
}

LotmlLoader::LotmlLoader(WidgetRegistry registry)
    : registry_(std::move(registry)) {
}

WidgetRegistry& LotmlLoader::registry() noexcept {
    return registry_;
}

const WidgetRegistry& LotmlLoader::registry() const noexcept {
    return registry_;
}

LoadedUi LotmlLoader::loadString(
    std::string_view xml,
    LoadOptions options) const {
    LotmlElement rootElement = parseDocument(xml);
    BuildContext::State state{&registry_, std::move(options), {}};
    BuildContext context(state);
    std::unique_ptr<Widget> root = context.buildChild(rootElement);
    return LoadedUi(
        std::make_unique<WidgetTree>(std::move(root)),
        std::move(state.widgetsById));
}

LoadedUi LotmlLoader::loadFile(
    const std::filesystem::path& file,
    LoadOptions options) const {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw LotmlError("failed to open LotML file: " + file.u8string());
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        throw LotmlError("failed to read LotML file: " + file.u8string());
    }
    return loadString(contents.str(), std::move(options));
}

WidgetRegistry createDefaultWidgetRegistry() {
    WidgetRegistry registry;
    registry.registerWidget(
        {"Row", true, linearProperties()},
        [](const LotmlElement& element, BuildContext& context) {
            return buildLinear(element, context, true);
        });
    registry.registerWidget(
        {"Column", true, linearProperties()},
        [](const LotmlElement& element, BuildContext& context) {
            return buildLinear(element, context, false);
        });
    registry.registerWidget(
        {
            "Label",
            false,
            {
                {"text", PropertyType::String, ""},
                {"fontFamily", PropertyType::String, "sans-serif"},
                {"fontSize", PropertyType::Number, "14"},
                {"fontWeight", PropertyType::Number, "400"},
                {"italic", PropertyType::Boolean, "false"},
                {"letterSpacing", PropertyType::Number, "0"},
                {"lineHeight", PropertyType::Number, "0"},
                {"color", PropertyType::Color, "#EDF2FFFF"},
                {"horizontalAlign", PropertyType::Enumeration, "start"},
                {"verticalAlign", PropertyType::Enumeration, "top"},
            },
        },
        buildLabel);
    registry.registerWidget(
        {
            "Box",
            false,
            {
                {"width", PropertyType::Number, "0"},
                {"height", PropertyType::Number, "0"},
                {"color", PropertyType::Color, "#00000000"},
                {"cornerRadius", PropertyType::Number, "0"},
            },
        },
        buildBox);
    registry.registerWidget(
        {
            "Button",
            true,
            {
                {"text", PropertyType::String, ""},
                {"fontFamily", PropertyType::String, "sans-serif"},
                {"fontSize", PropertyType::Number, "14"},
                {"width", PropertyType::Number, "120"},
                {"height", PropertyType::Number, "40"},
                {"onClick", PropertyType::Event, ""},
                {"normal", PropertyType::Color, "#2678F0FF"},
                {"hovered", PropertyType::Color, "#3B94FFFF"},
                {"pressed", PropertyType::Color, "#1459C7FF"},
                {"disabled", PropertyType::Color, "#383D47FF"},
                {"focusRing", PropertyType::Color, "#F5D151FF"},
                {"cornerRadius", PropertyType::Number, "8"},
                {"focusRingWidth", PropertyType::Number, "2"},
                {"contentPadding", PropertyType::Insets, "12,8,12,8"},
            },
        },
        buildButton);
    registry.registerWidget(
        {
            "Checkbox",
            true,
            {
                {"text", PropertyType::String, ""},
                {"fontFamily", PropertyType::String, "sans-serif"},
                {"fontSize", PropertyType::Number, "14"},
                {"checked", PropertyType::Boolean, "false"},
                {"enabled", PropertyType::Boolean, "true"},
                {"onChanged", PropertyType::Event, ""},
                {"unchecked", PropertyType::Color, "#292F40FF"},
                {"hovered", PropertyType::Color, "#38455CFF"},
                {"checkedColor", PropertyType::Color, "#2678F0FF"},
                {"checkmark", PropertyType::Color, "#F2F7FFFF"},
                {"textColor", PropertyType::Color, "#EDF2FFFF"},
                {"disabled", PropertyType::Color, "#40454FFF"},
                {"focusRing", PropertyType::Color, "#F5D151FF"},
                {"boxSize", PropertyType::Number, "22"},
                {"spacing", PropertyType::Number, "10"},
                {"cornerRadius", PropertyType::Number, "5"},
                {"focusRingWidth", PropertyType::Number, "2"},
            },
        },
        buildCheckbox);
    registry.registerWidget(
        {
            "NumericInput",
            false,
            {
                {"value", PropertyType::Number, "0"},
                {"minimum", PropertyType::Number, "0"},
                {"maximum", PropertyType::Number, "100"},
                {"step", PropertyType::Number, "1"},
                {"decimalPlaces", PropertyType::Number, "0"},
                {"width", PropertyType::Number, "160"},
                {"height", PropertyType::Number, "40"},
                {"fontFamily", PropertyType::String, "sans-serif"},
                {"fontSize", PropertyType::Number, "14"},
                {"enabled", PropertyType::Boolean, "true"},
                {"onChanged", PropertyType::Event, ""},
                {"normal", PropertyType::Color, "#1F2636FF"},
                {"hovered", PropertyType::Color, "#293347FF"},
                {"stepButton", PropertyType::Color, "#33425CFF"},
                {"stepButtonPressed", PropertyType::Color, "#1A5CB8FF"},
                {"indicator", PropertyType::Color, "#E8F0FFFF"},
                {"textColor", PropertyType::Color, "#EDF2FFFF"},
                {"disabled", PropertyType::Color, "#383D47FF"},
                {"focusRing", PropertyType::Color, "#F5D151FF"},
                {"contentPadding", PropertyType::Insets, "12,8,8,8"},
                {"stepButtonWidth", PropertyType::Number, "28"},
                {"cornerRadius", PropertyType::Number, "7"},
                {"focusRingWidth", PropertyType::Number, "2"},
            },
        },
        buildNumericInput);
    return registry;
}

} // namespace lotui::declarative
