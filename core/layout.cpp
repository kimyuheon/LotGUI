#include "core/layout.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace lotui {
namespace {

enum class Axis {
    Horizontal,
    Vertical,
};

constexpr float kLayoutEpsilon = 0.0001F;

float mainSize(Size size, Axis axis) noexcept {
    return axis == Axis::Horizontal ? size.width : size.height;
}

float crossSize(Size size, Axis axis) noexcept {
    return axis == Axis::Horizontal ? size.height : size.width;
}

float mainStart(const EdgeInsets& padding, Axis axis) noexcept {
    return axis == Axis::Horizontal ? padding.left : padding.top;
}

float mainPadding(const EdgeInsets& padding, Axis axis) noexcept {
    return axis == Axis::Horizontal
        ? padding.horizontal()
        : padding.vertical();
}

float crossStart(const EdgeInsets& padding, Axis axis) noexcept {
    return axis == Axis::Horizontal ? padding.top : padding.left;
}

float crossPadding(const EdgeInsets& padding, Axis axis) noexcept {
    return axis == Axis::Horizontal
        ? padding.vertical()
        : padding.horizontal();
}

Rect makeRect(
    float mainPosition,
    float crossPosition,
    float itemMainSize,
    float itemCrossSize,
    Axis axis) noexcept {
    if (axis == Axis::Horizontal) {
        return {
            mainPosition,
            crossPosition,
            itemMainSize,
            itemCrossSize,
        };
    }
    return {
        crossPosition,
        mainPosition,
        itemCrossSize,
        itemMainSize,
    };
}

bool validFiniteNonNegative(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F;
}

bool validMaximum(float value, float minimum) noexcept {
    return (std::isfinite(value) || std::isinf(value)) &&
        value >= minimum;
}

void validate(
    const LayoutConstraints& constraints,
    const std::vector<LayoutItem>& children,
    const LinearLayoutOptions& options) {
    if (!validFiniteNonNegative(constraints.minimum.width) ||
        !validFiniteNonNegative(constraints.minimum.height) ||
        !validMaximum(
            constraints.maximum.width, constraints.minimum.width) ||
        !validMaximum(
            constraints.maximum.height, constraints.minimum.height)) {
        throw std::invalid_argument("layout constraints are invalid");
    }

    if (!validFiniteNonNegative(options.spacing) ||
        !validFiniteNonNegative(options.padding.left) ||
        !validFiniteNonNegative(options.padding.top) ||
        !validFiniteNonNegative(options.padding.right) ||
        !validFiniteNonNegative(options.padding.bottom)) {
        throw std::invalid_argument("layout spacing and padding must be finite and non-negative");
    }

    for (const LayoutItem& child : children) {
        if (!validFiniteNonNegative(child.preferred.width) ||
            !validFiniteNonNegative(child.preferred.height) ||
            !validFiniteNonNegative(child.minimum.width) ||
            !validFiniteNonNegative(child.minimum.height) ||
            !validMaximum(child.maximum.width, child.minimum.width) ||
            !validMaximum(child.maximum.height, child.minimum.height) ||
            !validFiniteNonNegative(child.flex)) {
            throw std::invalid_argument("layout child dimensions or flex are invalid");
        }
    }
}

float clampedPreferred(
    const LayoutItem& child,
    Axis axis,
    bool main) noexcept {
    const float preferred = main
        ? mainSize(child.preferred, axis)
        : crossSize(child.preferred, axis);
    const float minimum = main
        ? mainSize(child.minimum, axis)
        : crossSize(child.minimum, axis);
    const float maximum = main
        ? mainSize(child.maximum, axis)
        : crossSize(child.maximum, axis);
    return std::clamp(preferred, minimum, maximum);
}

float distributeGrowth(
    std::vector<float>& sizes,
    const std::vector<LayoutItem>& children,
    Axis axis,
    float available) {
    float remaining = available;
    while (remaining > kLayoutEpsilon) {
        float totalFlex = 0.0F;
        for (std::size_t index = 0; index < children.size(); ++index) {
            const float maximum = mainSize(children[index].maximum, axis);
            if (children[index].flex > 0.0F &&
                sizes[index] + kLayoutEpsilon < maximum) {
                totalFlex += children[index].flex;
            }
        }
        if (totalFlex <= 0.0F) {
            break;
        }

        const float passAvailable = remaining;
        float consumed = 0.0F;
        for (std::size_t index = 0; index < children.size(); ++index) {
            const float maximum = mainSize(children[index].maximum, axis);
            if (children[index].flex <= 0.0F ||
                sizes[index] + kLayoutEpsilon >= maximum) {
                continue;
            }
            const float share = passAvailable *
                children[index].flex / totalFlex;
            const float growth = std::min(share, maximum - sizes[index]);
            sizes[index] += growth;
            consumed += growth;
        }
        if (consumed <= kLayoutEpsilon) {
            break;
        }
        remaining -= consumed;
    }
    return remaining;
}

float distributeShrink(
    std::vector<float>& sizes,
    const std::vector<LayoutItem>& children,
    Axis axis,
    float deficit,
    bool flexOnly) {
    float remaining = deficit;
    while (remaining > kLayoutEpsilon) {
        float totalWeight = 0.0F;
        for (std::size_t index = 0; index < children.size(); ++index) {
            const float minimum = mainSize(children[index].minimum, axis);
            if (sizes[index] > minimum + kLayoutEpsilon &&
                (!flexOnly || children[index].flex > 0.0F)) {
                totalWeight += flexOnly ? children[index].flex : 1.0F;
            }
        }
        if (totalWeight <= 0.0F) {
            break;
        }

        const float passDeficit = remaining;
        float consumed = 0.0F;
        for (std::size_t index = 0; index < children.size(); ++index) {
            const float minimum = mainSize(children[index].minimum, axis);
            if (sizes[index] <= minimum + kLayoutEpsilon ||
                (flexOnly && children[index].flex <= 0.0F)) {
                continue;
            }
            const float weight = flexOnly ? children[index].flex : 1.0F;
            const float share = passDeficit * weight / totalWeight;
            const float shrink = std::min(share, sizes[index] - minimum);
            sizes[index] -= shrink;
            consumed += shrink;
        }
        if (consumed <= kLayoutEpsilon) {
            break;
        }
        remaining -= consumed;
    }
    return remaining;
}

LayoutResult layoutLinear(
    Axis axis,
    const LayoutConstraints& constraints,
    const std::vector<LayoutItem>& children,
    const LinearLayoutOptions& options) {
    validate(constraints, children, options);

    const float baseSpacing = children.size() > 1
        ? options.spacing * static_cast<float>(children.size() - 1)
        : 0.0F;
    std::vector<float> childMainSizes(children.size());
    float naturalChildrenMain = 0.0F;
    float naturalCross = 0.0F;
    for (std::size_t index = 0; index < children.size(); ++index) {
        childMainSizes[index] = clampedPreferred(children[index], axis, true);
        naturalChildrenMain += childMainSizes[index];
        naturalCross = std::max(
            naturalCross,
            clampedPreferred(children[index], axis, false));
    }

    float desiredMain = naturalChildrenMain + baseSpacing +
        mainPadding(options.padding, axis);
    const float maximumMain = mainSize(constraints.maximum, axis);
    if (options.mainAxisSize == MainAxisSize::Max &&
        std::isfinite(maximumMain)) {
        desiredMain = maximumMain;
    }

    const float desiredCross = naturalCross +
        crossPadding(options.padding, axis);
    const Size desiredSize = axis == Axis::Horizontal
        ? Size{desiredMain, desiredCross}
        : Size{desiredCross, desiredMain};
    const Size containerSize = constraints.constrain(desiredSize);
    const float containerMain = mainSize(containerSize, axis);
    const float containerCross = crossSize(containerSize, axis);
    const float availableChildrenMain = std::max(
        0.0F,
        containerMain - mainPadding(options.padding, axis) - baseSpacing);

    float allocatedMain = std::accumulate(
        childMainSizes.begin(), childMainSizes.end(), 0.0F);
    if (availableChildrenMain > allocatedMain + kLayoutEpsilon) {
        distributeGrowth(
            childMainSizes,
            children,
            axis,
            availableChildrenMain - allocatedMain);
    } else if (allocatedMain > availableChildrenMain + kLayoutEpsilon) {
        float deficit = distributeShrink(
            childMainSizes,
            children,
            axis,
            allocatedMain - availableChildrenMain,
            true);
        if (deficit > kLayoutEpsilon) {
            distributeShrink(
                childMainSizes, children, axis, deficit, false);
        }
    }

    allocatedMain = std::accumulate(
        childMainSizes.begin(), childMainSizes.end(), 0.0F);
    const float freeMain = std::max(
        0.0F, availableChildrenMain - allocatedMain);
    float leadingMain = 0.0F;
    float spacing = options.spacing;
    switch (options.mainAxisAlignment) {
    case MainAxisAlignment::Start:
        break;
    case MainAxisAlignment::Center:
        leadingMain = freeMain * 0.5F;
        break;
    case MainAxisAlignment::End:
        leadingMain = freeMain;
        break;
    case MainAxisAlignment::SpaceBetween:
        if (children.size() > 1) {
            spacing += freeMain /
                static_cast<float>(children.size() - 1);
        }
        break;
    }

    const float availableCross = std::max(
        0.0F, containerCross - crossPadding(options.padding, axis));
    LayoutResult result;
    result.size = containerSize;
    result.children.reserve(children.size());

    float position = mainStart(options.padding, axis) + leadingMain;
    for (std::size_t index = 0; index < children.size(); ++index) {
        const float minimumCross = crossSize(children[index].minimum, axis);
        const float maximumCross = crossSize(children[index].maximum, axis);
        float itemCross = clampedPreferred(children[index], axis, false);
        if (options.crossAxisAlignment == CrossAxisAlignment::Stretch) {
            itemCross = std::clamp(
                availableCross, minimumCross, maximumCross);
        }

        const float crossFree = std::max(0.0F, availableCross - itemCross);
        float crossOffset = 0.0F;
        if (options.crossAxisAlignment == CrossAxisAlignment::Center) {
            crossOffset = crossFree * 0.5F;
        } else if (options.crossAxisAlignment == CrossAxisAlignment::End) {
            crossOffset = crossFree;
        }

        result.children.push_back(makeRect(
            position,
            crossStart(options.padding, axis) + crossOffset,
            childMainSizes[index],
            itemCross,
            axis));
        position += childMainSizes[index] + spacing;
    }
    return result;
}

} // namespace

Size LayoutConstraints::constrain(Size size) const noexcept {
    return {
        std::clamp(size.width, minimum.width, maximum.width),
        std::clamp(size.height, minimum.height, maximum.height),
    };
}

LayoutResult layoutRow(
    const LayoutConstraints& constraints,
    const std::vector<LayoutItem>& children,
    const LinearLayoutOptions& options) {
    return layoutLinear(Axis::Horizontal, constraints, children, options);
}

LayoutResult layoutColumn(
    const LayoutConstraints& constraints,
    const std::vector<LayoutItem>& children,
    const LinearLayoutOptions& options) {
    return layoutLinear(Axis::Vertical, constraints, children, options);
}

} // namespace lotui
