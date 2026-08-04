#pragma once

#include "core/widget.h"

namespace lotui {

class Box final : public Widget {
public:
    explicit Box(
        Size preferredSize = {},
        Color color = {},
        float cornerRadius = 0.0F);

    Size measure(const LayoutConstraints& constraints) const override;

    void setPreferredSize(Size preferredSize) noexcept;
    Size preferredSize() const noexcept;

    void setColor(Color color) noexcept;
    Color color() const noexcept;

    void setCornerRadius(float cornerRadius) noexcept;
    float cornerRadius() const noexcept;

    void setTexture(
        TextureId texture,
        Rect textureCoordinates = {0.0F, 0.0F, 1.0F, 1.0F}) noexcept;
    TextureId texture() const noexcept;
    Rect textureCoordinates() const noexcept;

protected:
    void onPaint(std::vector<PaintCommand>& commands) const override;

private:
    Size preferredSize_{};
    Color color_{};
    float cornerRadius_{0.0F};
    TextureId texture_{invalidTextureId};
    Rect textureCoordinates_{0.0F, 0.0F, 1.0F, 1.0F};
};

} // namespace lotui
