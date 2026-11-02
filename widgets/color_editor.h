#pragma once

#include "core/widget.h"
#include "text/text_layout.h"

#include <array>
#include <cstddef>
#include <functional>
#include <memory>

namespace lotui {

struct ColorEditorStyle {
    Color background{0.018F, 0.024F, 0.035F, 1.0F};
    Color track{0.060F, 0.075F, 0.10F, 1.0F};
    Color text{0.91F, 0.94F, 0.97F, 1.0F};
    Color thumb{0.93F, 0.95F, 0.97F, 1.0F};
    Color focusRing{0.96F, 0.82F, 0.32F, 1.0F};
    Color disabled{0.20F, 0.22F, 0.25F, 1.0F};
    std::array<Color, 3> channels{{
        {0.88F, 0.18F, 0.18F, 1.0F},
        {0.24F, 0.72F, 0.34F, 1.0F},
        {0.26F, 0.48F, 0.91F, 1.0F}}};
    float cornerRadius{6.0F};
};

// RGB channels are normalized to [0, 1]; alpha is preserved but not edited.
class ColorEditor final : public Widget {
public:
    using ChangedHandler = std::function<void(Color)>;
    using EditHandler = std::function<void()>;

    ColorEditor(std::shared_ptr<const TextEngine> textEngine,
        Color color = {1.0F, 1.0F, 1.0F, 1.0F},
        ChangedHandler onChanged = {},
        Size preferredSize = {240.0F, 100.0F},
        ColorEditorStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;
    void setColor(Color color);
    Color color() const noexcept;
    void setOnChanged(ChangedHandler handler);
    void setOnEditStarted(EditHandler handler);
    void setOnEditFinished(EditHandler handler);
    void setEnabled(bool enabled);
    bool isEnabled() const noexcept;
    void setPreferredSize(Size size) noexcept;
    Size preferredSize() const noexcept;
    void setStyle(ColorEditorStyle style) noexcept;
    const ColorEditorStyle& style() const noexcept;
    std::size_t activeChannel() const noexcept;
    Rect channelTrackBounds(std::size_t channel) const noexcept;
    Rect previewBounds() const noexcept;

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;

private:
    void applyUserChannel(std::size_t channel, float value);
    void applyPointer(float x);
    void refreshValueLayout(std::size_t channel);
    void finishEdit();
    float channelValue(std::size_t channel) const noexcept;
    std::size_t hitChannel(Point position) const noexcept;

    std::shared_ptr<const TextEngine> textEngine_;
    Color color_{};
    ChangedHandler onChanged_{};
    EditHandler onEditStarted_{};
    EditHandler onEditFinished_{};
    Size preferredSize_{};
    ColorEditorStyle style_{};
    TextStyle textStyle_{};
    std::array<std::unique_ptr<TextLayout>, 3> labels_{};
    std::array<std::unique_ptr<TextLayout>, 3> values_{};
    std::size_t activeChannel_{0};
    bool enabled_{true};
    bool focused_{false};
    bool dragging_{false};
};

} // namespace lotui
