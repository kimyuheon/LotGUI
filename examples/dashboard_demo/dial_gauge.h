#pragma once

#include "examples/common/desktop_palette.h"
#include "core/widget.h"
#include "text/text_layout.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lotui::example {

class DialGauge final : public lotui::Widget {
public:
    DialGauge(std::shared_ptr<const lotui::TextEngine> text, int maximum,
        std::string unit, lotui::Color accent)
        : text_(std::move(text)), maximum_(maximum), unit_(std::move(unit)),
          accent_(accent) {}

    void setValue(int value) { value_ = value; }
    void clearValue() noexcept { value_.reset(); }
    int value() const noexcept { return value_.value_or(0); }
    lotui::Size measure(const lotui::LayoutConstraints& constraints) const override {
        return constraints.constrain({206.0F, 160.0F});
    }

protected:
    void onPaint(std::vector<lotui::PaintCommand>& commands) const override {
        const auto area = bounds();
        const float radius = std::min(area.width * 0.42F, area.height * 0.43F);
        if (radius <= 0.0F) return;
        const lotui::Point center{area.x + area.width * 0.5F,
            area.y + area.height * 0.51F};
        constexpr float pi = 3.14159265358979323846F;
        const auto locate = [center](float distance, float fraction) {
            const float theta = pi * (5.0F / 6.0F + 4.0F / 3.0F * fraction);
            return lotui::Point{center.x + distance * std::cos(theta),
                center.y + distance * std::sin(theta)};
        };
        const auto stroke = [&](lotui::Point a, lotui::Point b,
                                float width, lotui::Color color) {
            commands.push_back(lotui::linePaintCommand(a, b, width, clip(), color));
        };
        const auto centeredText = [&](std::string value, float x, float y,
                                      float size, lotui::Color color) {
            lotui::TextStyle style;
            style.fontFamilies = {"Noto Sans KR"};
            style.fontSize = size;
            auto layout = text_->createLayout(value, style, {});
            layout->appendPaintCommands({x - layout->size().width * 0.5F, y},
                clip(), color, commands);
        };
        const auto track = desktopColor(0x394656);
        const auto tick = desktopColor(0x8494a7);
        const float fraction = std::clamp(static_cast<float>(value()) / maximum_, 0.0F, 1.0F);
        for (int i = 0; i < 48; ++i) {
            const float from = i / 48.0F;
            const float to = (i + 1) / 48.0F;
            const float join = std::max(0.0F, from - 0.004F);
            stroke(locate(radius, join), locate(radius, to), 4, track);
        }
        for (int i = 0; i < 48; ++i) {
            const float from = i / 48.0F;
            if (from >= fraction) break;
            stroke(locate(radius, std::max(0.0F, from - 0.004F)),
                locate(radius, std::min((i + 1) / 48.0F, fraction)), 4, accent_);
        }
        for (int i = 0; i <= 20; ++i) {
            const float at = i / 20.0F;
            const float length = i % 5 == 0 ? 10.0F : 5.0F;
            stroke(locate(radius - 7, at), locate(radius - 7 - length, at),
                i % 5 == 0 ? 1.75F : 1.0F, tick);
        }
        if (value_) {
            stroke(locate(-10, fraction), locate(radius - 22, fraction), 2.5F,
                desktopColor(0xf0f4fa));
            commands.push_back({{center.x - 4, center.y - 4, 8, 8}, clip(),
                accent_, lotui::invalidTextureId, 4.0F});
        }
        const float scaleY = center.y + radius * 0.50F + 5.0F;
        centeredText("0", center.x - radius * 1.06F, scaleY, 11, tick);
        centeredText(std::to_string(maximum_), center.x + radius * 1.06F,
            scaleY, 11, tick);
        centeredText(value_ ? std::to_string(*value_) + " " + unit_ : "—", center.x,
            center.y + radius * 0.40F, 19, desktopColor(0xe9eff7));
    }

private:
    std::shared_ptr<const lotui::TextEngine> text_;
    int maximum_;
    std::string unit_;
    lotui::Color accent_;
    std::optional<int> value_;
};

} // namespace lotui::example
