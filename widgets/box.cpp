#include "widgets/box.h"

#include <algorithm>

namespace lotui {

Box::Box(Size preferredSize, Color color, float cornerRadius)
    : preferredSize_(preferredSize),
      color_(color),
      cornerRadius_(std::max(0.0F, cornerRadius)) {
}

Size Box::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain(preferredSize_);
}

void Box::setPreferredSize(Size preferredSize) noexcept {
    preferredSize_ = preferredSize;
}

Size Box::preferredSize() const noexcept {
    return preferredSize_;
}

void Box::setColor(Color color) noexcept {
    color_ = color;
}

Color Box::color() const noexcept {
    return color_;
}

void Box::setCornerRadius(float cornerRadius) noexcept {
    cornerRadius_ = std::max(0.0F, cornerRadius);
}

float Box::cornerRadius() const noexcept {
    return cornerRadius_;
}

void Box::setTexture(
    TextureId texture,
    Rect textureCoordinates) noexcept {
    texture_ = texture;
    textureCoordinates_ = textureCoordinates;
}

TextureId Box::texture() const noexcept {
    return texture_;
}

Rect Box::textureCoordinates() const noexcept {
    return textureCoordinates_;
}

void Box::onPaint(std::vector<PaintCommand>& commands) const {
    commands.push_back({
        bounds(), clip(), color_, texture_, cornerRadius_,
        textureCoordinates_});
}

} // namespace lotui
