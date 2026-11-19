#include "widgets/property_field.h"

#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/text_field.h"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace lotui {
namespace {
class ExpressionParser {
public:
    explicit ExpressionParser(std::string_view text) : text_(text) {}
    std::optional<double> parse() {
        const auto value = sum();
        spaces();
        if (!value || position_ != text_.size() || !std::isfinite(*value))
            return std::nullopt;
        return value;
    }
private:
    void spaces() {
        while (position_ < text_.size() &&
            (text_[position_] == ' ' || text_[position_] == '\t')) ++position_;
    }
    bool consume(char ch) {
        spaces();
        if (position_ >= text_.size() || text_[position_] != ch) return false;
        ++position_;
        return true;
    }
    std::optional<double> sum() {
        auto lhs = product();
        while (lhs) {
            if (consume('+')) {
                auto rhs = product();
                if (!rhs) return std::nullopt;
                *lhs += *rhs;
            } else if (consume('-')) {
                auto rhs = product();
                if (!rhs) return std::nullopt;
                *lhs -= *rhs;
            } else break;
        }
        return lhs;
    }
    std::optional<double> product() {
        auto lhs = factor();
        while (lhs) {
            if (consume('*')) {
                auto rhs = factor();
                if (!rhs) return std::nullopt;
                *lhs *= *rhs;
            } else if (consume('/')) {
                auto rhs = factor();
                if (!rhs || *rhs == 0.0) return std::nullopt;
                *lhs /= *rhs;
            } else break;
        }
        return lhs;
    }
    std::optional<double> factor() {
        if (consume('+')) return factor();
        if (consume('-')) {
            auto value = factor();
            if (value) *value = -*value;
            return value;
        }
        if (consume('(')) {
            auto value = sum();
            if (!value || !consume(')')) return std::nullopt;
            return value;
        }
        spaces();
        const auto start = position_;
        bool digit = false;
        while (position_ < text_.size() && text_[position_] >= '0' &&
                text_[position_] <= '9') { ++position_; digit = true; }
        if (position_ < text_.size() && text_[position_] == '.') {
            ++position_;
            while (position_ < text_.size() && text_[position_] >= '0' &&
                    text_[position_] <= '9') { ++position_; digit = true; }
        }
        if (!digit) return std::nullopt;
        if (position_ < text_.size() &&
                (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_;
            if (position_ < text_.size() &&
                    (text_[position_] == '+' || text_[position_] == '-')) ++position_;
            const auto exponentStart = position_;
            while (position_ < text_.size() && text_[position_] >= '0' &&
                    text_[position_] <= '9') ++position_;
            if (exponentStart == position_) return std::nullopt;
        }
        std::istringstream number(std::string{text_.substr(start,
            position_ - start)});
        number.imbue(std::locale::classic());
        double value = 0.0;
        char trailing = 0;
        if (!(number >> value) || number >> trailing || !std::isfinite(value))
            return std::nullopt;
        return value;
    }
    std::string_view text_;
    std::size_t position_{0};
};

std::string formatNumber(double value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(10) << value;
    return output.str();
}

std::unique_ptr<Label> makeLabel(const std::shared_ptr<const TextEngine>& text,
        std::string label, const TextStyle& style) {
    auto result = std::make_unique<Label>(text, std::move(label), style);
    result->setVerticalAlignment(VerticalTextAlignment::Center);
    return result;
}
} // namespace

std::optional<double> evaluatePropertyExpression(std::string_view expression) {
    if (expression.size() > 256) return std::nullopt;
    return ExpressionParser(expression).parse();
}

PropertyFieldStyle::PropertyFieldStyle() {
    input.normal = {0.034F, 0.045F, 0.061F, 1.0F};
    input.hovered = {0.047F, 0.064F, 0.087F, 1.0F};
    input.cornerRadius = 3.0F;
    input.contentPadding = {8.0F, 4.0F, 8.0F, 4.0F};
    readOnlyInput = input;
    readOnlyInput.normal = {0.028F, 0.036F, 0.048F, 1.0F};
    readOnlyInput.hovered = readOnlyInput.normal;
    readOnlyInput.text = {0.66F, 0.72F, 0.77F, 1.0F};
    action.normal = {0.046F, 0.061F, 0.078F, 1.0F};
    action.hovered = {0.069F, 0.089F, 0.113F, 1.0F};
    action.pressed = {0.029F, 0.042F, 0.061F, 1.0F};
    action.cornerRadius = 3.0F;
    action.contentPadding = {2.0F, 2.0F, 2.0F, 2.0F};
}

PropertyField::PropertyField(std::shared_ptr<const TextEngine> textEngine,
        PropertyFieldOptions options, std::string value,
        ValueHandler onValue, PickHandler onPick, ErrorHandler onError,
        TextStyle textStyle)
    : LinearLayout(true), options_(std::move(options)),
      onValue_(std::move(onValue)), onPick_(std::move(onPick)),
      onError_(std::move(onError)) {
    LinearLayoutOptions layout;
    layout.spacing = 5.0F;
    layout.crossAxisAlignment = CrossAxisAlignment::Center;
    setOptions(layout);
    auto title = makeLabel(textEngine, options_.label, textStyle);
    label_ = title.get();
    addChild(std::move(title),
        {0.0F, {96.0F, 30.0F}, {96.0F, 30.0F}});
    auto field = std::make_unique<TextField>(textEngine, std::move(value),
        Size{150.0F, 30.0F}, TextField::ChangedHandler{},
        [this](const std::string&) { submit(); }, style_.input, textStyle);
    input_ = field.get();
    addChild(std::move(field),
        {1.0F, {100.0F, 30.0F}, {unboundedLayoutSize, 30.0F}});
    if (options_.pointPicker) {
        auto icon = makeLabel(textEngine, "◎", textStyle);
        icon->setHorizontalAlignment(HorizontalTextAlignment::Center);
        pickLabel_ = icon.get();
        auto button = std::make_unique<Button>(
            std::move(icon), Size{32.0F, 30.0F},
            [this] { requestPick(); }, style_.action);
        pickButton_ = button.get();
        addChild(std::move(button),
            {0.0F, {32.0F, 30.0F}, {32.0F, 30.0F}});
    }
    if (options_.calculator) {
        auto icon = makeLabel(textEngine, "=", textStyle);
        icon->setHorizontalAlignment(HorizontalTextAlignment::Center);
        calcLabel_ = icon.get();
        auto button = std::make_unique<Button>(
            std::move(icon), Size{32.0F, 30.0F},
            [this] { submit(); }, style_.action);
        calcButton_ = button.get();
        addChild(std::move(button),
            {0.0F, {32.0F, 30.0F}, {32.0F, 30.0F}});
    }
    const auto access = options_.access;
    options_.access = PropertyAccess::ReadWrite;
    setAccess(access);
    setStyle(style_);
}

PropertyAccess PropertyField::access() const noexcept { return options_.access; }
void PropertyField::setAccess(PropertyAccess access) {
    if (access == PropertyAccess::WriteOnly &&
            options_.access != PropertyAccess::WriteOnly) input_->setText({});
    options_.access = access;
    input_->setReadOnly(access == PropertyAccess::ReadOnly);
    input_->setStyle(access == PropertyAccess::ReadOnly ?
        style_.readOnlyInput : style_.input);
    label_->setColor(access == PropertyAccess::ReadOnly ?
        style_.readOnlyLabel : style_.label);
    if (pickButton_) pickButton_->setEnabled(access != PropertyAccess::ReadOnly);
    if (calcButton_) calcButton_->setEnabled(access != PropertyAccess::ReadOnly);
}
void PropertyField::setValue(std::string value) {
    if (options_.access != PropertyAccess::WriteOnly)
        input_->setText(std::move(value));
}
std::optional<std::string> PropertyField::value() const {
    if (options_.access == PropertyAccess::WriteOnly) return std::nullopt;
    return input_->text();
}
void PropertyField::reportError(std::string_view message) {
    if (onError_) onError_(message);
}
bool PropertyField::submit() {
    if (options_.access == PropertyAccess::ReadOnly) return false;
    std::string value = input_->text();
    if (options_.calculator) {
        const auto result = evaluatePropertyExpression(value);
        if (!result) {
            reportError("Invalid expression");
            return false;
        }
        value = formatNumber(*result);
    }
    if (value.empty()) return false;
    if (onValue_) onValue_(value);
    input_->setText(options_.access == PropertyAccess::WriteOnly ?
        std::string{} : value);
    return true;
}
bool PropertyField::acceptPoint(PropertyPoint point) {
    if (!options_.pointPicker || options_.access == PropertyAccess::ReadOnly ||
            !std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z)) return false;
    const std::string value = formatNumber(point.x) + ", " +
        formatNumber(point.y) + ", " + formatNumber(point.z);
    if (onValue_) onValue_(value);
    input_->setText(options_.access == PropertyAccess::WriteOnly ?
        std::string{} : value);
    return true;
}
void PropertyField::requestPick() {
    if (options_.pointPicker && options_.access != PropertyAccess::ReadOnly &&
            onPick_) onPick_();
}
void PropertyField::setStyle(PropertyFieldStyle style) {
    style_ = std::move(style);
    input_->setStyle(options_.access == PropertyAccess::ReadOnly ?
        style_.readOnlyInput : style_.input);
    label_->setColor(options_.access == PropertyAccess::ReadOnly ?
        style_.readOnlyLabel : style_.label);
    if (pickButton_) pickButton_->setStyle(style_.action);
    if (calcButton_) calcButton_->setStyle(style_.action);
    if (pickLabel_) pickLabel_->setColor(style_.actionText);
    if (calcLabel_) calcLabel_->setColor(style_.actionText);
}
const PropertyFieldStyle& PropertyField::style() const noexcept { return style_; }
TextField& PropertyField::input() noexcept { return *input_; }
const TextField& PropertyField::input() const noexcept { return *input_; }

} // namespace lotui
