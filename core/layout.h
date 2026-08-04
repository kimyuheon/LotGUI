#pragma once

#include "core/geometry.h"

#include <limits>
#include <vector>

namespace lotui {

inline constexpr float unboundedLayoutSize =
    std::numeric_limits<float>::infinity();

struct EdgeInsets {
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};

    static constexpr EdgeInsets all(float value) noexcept {
        return {value, value, value, value};
    }

    static constexpr EdgeInsets symmetric(
        float horizontal,
        float vertical) noexcept {
        return {horizontal, vertical, horizontal, vertical};
    }

    constexpr float horizontal() const noexcept {
        return left + right;
    }

    constexpr float vertical() const noexcept {
        return top + bottom;
    }
};

struct LayoutConstraints {
    Size minimum{};
    Size maximum{unboundedLayoutSize, unboundedLayoutSize};

    static constexpr LayoutConstraints tight(Size size) noexcept {
        return {size, size};
    }

    static constexpr LayoutConstraints loose(Size maximumSize) noexcept {
        return {{}, maximumSize};
    }

    Size constrain(Size size) const noexcept;
};

struct LayoutItem {
    Size preferred{};
    Size minimum{};
    Size maximum{unboundedLayoutSize, unboundedLayoutSize};
    float flex{0.0F};
};

enum class MainAxisSize {
    Min,
    Max,
};

enum class MainAxisAlignment {
    Start,
    Center,
    End,
    SpaceBetween,
};

enum class CrossAxisAlignment {
    Start,
    Center,
    End,
    Stretch,
};

struct LinearLayoutOptions {
    float spacing{0.0F};
    EdgeInsets padding{};
    MainAxisSize mainAxisSize{MainAxisSize::Max};
    MainAxisAlignment mainAxisAlignment{MainAxisAlignment::Start};
    CrossAxisAlignment crossAxisAlignment{CrossAxisAlignment::Start};
};

struct LayoutResult {
    Size size{};
    std::vector<Rect> children;
};

LayoutResult layoutRow(
    const LayoutConstraints& constraints,
    const std::vector<LayoutItem>& children,
    const LinearLayoutOptions& options = {});

LayoutResult layoutColumn(
    const LayoutConstraints& constraints,
    const std::vector<LayoutItem>& children,
    const LinearLayoutOptions& options = {});

} // namespace lotui
