#pragma once

#include "core/color.h"
#include "core/geometry.h"
#include "core/layout.h"
#include "core/paint_command.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lotui {

enum class FontWeight : std::uint16_t {
    Thin = 100,
    ExtraLight = 200,
    Light = 300,
    Normal = 400,
    Medium = 500,
    SemiBold = 600,
    Bold = 700,
    ExtraBold = 800,
    Black = 900,
};

struct TextStyle {
    std::vector<std::string> fontFamilies{"sans-serif"};
    float fontSize{14.0F};
    FontWeight weight{FontWeight::Normal};
    bool italic{false};
    float letterSpacing{0.0F};
    float lineHeight{0.0F};
};

struct TextLayoutOptions {
    float maximumWidth{unboundedLayoutSize};
    std::size_t maximumLines{0};
    bool wrap{false};
};

bool isValidTextStyle(const TextStyle& style) noexcept;
bool isValidTextLayoutOptions(const TextLayoutOptions& options) noexcept;

class TextLayout {
public:
    virtual ~TextLayout() = default;

    TextLayout(const TextLayout&) = delete;
    TextLayout& operator=(const TextLayout&) = delete;
    TextLayout(TextLayout&&) = delete;
    TextLayout& operator=(TextLayout&&) = delete;

    virtual Size size() const noexcept = 0;
    virtual float baseline() const noexcept = 0;
    virtual void appendPaintCommands(
        Point origin,
        Rect clip,
        Color color,
        std::vector<PaintCommand>& commands) const = 0;

protected:
    TextLayout() = default;
};

class TextEngine {
public:
    virtual ~TextEngine() = default;

    TextEngine(const TextEngine&) = delete;
    TextEngine& operator=(const TextEngine&) = delete;
    TextEngine(TextEngine&&) = delete;
    TextEngine& operator=(TextEngine&&) = delete;

    virtual std::unique_ptr<TextLayout> createLayout(
        std::string_view utf8Text,
        const TextStyle& style,
        const TextLayoutOptions& options) const = 0;

protected:
    TextEngine() = default;
};

} // namespace lotui
