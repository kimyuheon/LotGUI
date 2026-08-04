#include "widgets/label.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {

Label::Label(
    std::shared_ptr<const TextEngine> textEngine,
    std::string text,
    TextStyle style)
    : textEngine_(std::move(textEngine)),
      text_(std::move(text)),
      style_(std::move(style)) {
    if (!textEngine_) {
        throw std::invalid_argument("Label requires a TextEngine");
    }
    if (!isValidTextStyle(style_)) {
        throw std::invalid_argument("Label text style is invalid");
    }
}

Size Label::measure(const LayoutConstraints& constraints) const {
    const TextLayout& layout = ensureLayout(constraints.maximum.width);
    return constraints.constrain(layout.size());
}

void Label::setText(std::string text) {
    if (text_ == text) {
        return;
    }
    text_ = std::move(text);
    invalidateLayout();
}

const std::string& Label::text() const noexcept {
    return text_;
}

void Label::setTextStyle(TextStyle style) {
    if (!isValidTextStyle(style)) {
        throw std::invalid_argument("Label text style is invalid");
    }
    style_ = std::move(style);
    invalidateLayout();
}

const TextStyle& Label::textStyle() const noexcept {
    return style_;
}

void Label::setColor(Color color) noexcept {
    color_ = color;
}

Color Label::color() const noexcept {
    return color_;
}

void Label::setHorizontalAlignment(
    HorizontalTextAlignment alignment) noexcept {
    horizontalAlignment_ = alignment;
}

HorizontalTextAlignment Label::horizontalAlignment() const noexcept {
    return horizontalAlignment_;
}

void Label::setVerticalAlignment(VerticalTextAlignment alignment) noexcept {
    verticalAlignment_ = alignment;
}

VerticalTextAlignment Label::verticalAlignment() const noexcept {
    return verticalAlignment_;
}

void Label::onPaint(std::vector<PaintCommand>& commands) const {
    const TextLayout& layout = ensureLayout(bounds().width);
    const Size textSize = layout.size();
    float x = bounds().x;
    float y = bounds().y;

    if (horizontalAlignment_ == HorizontalTextAlignment::Center) {
        x += std::max(0.0F, bounds().width - textSize.width) * 0.5F;
    } else if (horizontalAlignment_ == HorizontalTextAlignment::End) {
        x += std::max(0.0F, bounds().width - textSize.width);
    }
    if (verticalAlignment_ == VerticalTextAlignment::Center) {
        y += std::max(0.0F, bounds().height - textSize.height) * 0.5F;
    } else if (verticalAlignment_ == VerticalTextAlignment::Bottom) {
        y += std::max(0.0F, bounds().height - textSize.height);
    }
    layout.appendPaintCommands({x, y}, clip(), color_, commands);
}

const TextLayout& Label::ensureLayout(float maximumWidth) const {
    const float normalizedWidth = std::isfinite(maximumWidth)
        ? std::max(0.0F, maximumWidth)
        : unboundedLayoutSize;
    if (!layout_ || layoutWidth_ != normalizedWidth) {
        TextLayoutOptions options;
        options.maximumWidth = normalizedWidth;
        options.wrap = std::isfinite(normalizedWidth);
        layout_ = textEngine_->createLayout(text_, style_, options);
        if (!layout_) {
            throw std::runtime_error("TextEngine returned a null layout");
        }
        layoutWidth_ = normalizedWidth;
    }
    return *layout_;
}

void Label::invalidateLayout() noexcept {
    layout_.reset();
    layoutWidth_ = -1.0F;
}

} // namespace lotui
