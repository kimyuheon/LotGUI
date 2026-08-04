#pragma once

#include <algorithm>

namespace lotui {

struct Point {
    float x{0.0F};
    float y{0.0F};
};

struct Size {
    float width{0.0F};
    float height{0.0F};
};

struct Rect {
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};
};

constexpr bool hasArea(const Rect& rectangle) noexcept {
    return rectangle.width > 0.0F && rectangle.height > 0.0F;
}

constexpr bool contains(const Rect& rectangle, Point point) noexcept {
    return hasArea(rectangle) &&
        point.x >= rectangle.x && point.y >= rectangle.y &&
        point.x < rectangle.x + rectangle.width &&
        point.y < rectangle.y + rectangle.height;
}

inline Rect intersect(const Rect& left, const Rect& right) noexcept {
    const float x = std::max(left.x, right.x);
    const float y = std::max(left.y, right.y);
    const float rightEdge = std::min(
        left.x + std::max(0.0F, left.width),
        right.x + std::max(0.0F, right.width));
    const float bottomEdge = std::min(
        left.y + std::max(0.0F, left.height),
        right.y + std::max(0.0F, right.height));
    return {
        x,
        y,
        std::max(0.0F, rightEdge - x),
        std::max(0.0F, bottomEdge - y),
    };
}

constexpr Rect translated(Rect rectangle, Point offset) noexcept {
    rectangle.x += offset.x;
    rectangle.y += offset.y;
    return rectangle;
}

} // namespace lotui
