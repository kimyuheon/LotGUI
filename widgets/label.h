#pragma once

#include "core/widget.h"
#include "text/text_layout.h"

#include <memory>
#include <string>

namespace lotui {

enum class HorizontalTextAlignment {
    Start,
    Center,
    End,
};

enum class VerticalTextAlignment {
    Top,
    Center,
    Bottom,
};

class Label final : public Widget {
public:
    Label(
        std::shared_ptr<const TextEngine> textEngine,
        std::string text = {},
        TextStyle style = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setText(std::string text);
    const std::string& text() const noexcept;

    void setTextStyle(TextStyle style);
    const TextStyle& textStyle() const noexcept;

    void setColor(Color color) noexcept;
    Color color() const noexcept;

    void setHorizontalAlignment(HorizontalTextAlignment alignment) noexcept;
    HorizontalTextAlignment horizontalAlignment() const noexcept;
    void setVerticalAlignment(VerticalTextAlignment alignment) noexcept;
    VerticalTextAlignment verticalAlignment() const noexcept;

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;

private:
    const TextLayout& ensureLayout(float maximumWidth) const;
    void invalidateLayout() noexcept;

    std::shared_ptr<const TextEngine> textEngine_;
    std::string text_;
    TextStyle style_{};
    Color color_{0.93F, 0.95F, 1.0F, 1.0F};
    HorizontalTextAlignment horizontalAlignment_{
        HorizontalTextAlignment::Start};
    VerticalTextAlignment verticalAlignment_{VerticalTextAlignment::Top};
    mutable float layoutWidth_{-1.0F};
    mutable std::unique_ptr<TextLayout> layout_;
};

} // namespace lotui
