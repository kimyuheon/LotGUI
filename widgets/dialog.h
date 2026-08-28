#pragma once

#include "widgets/single_child_widget.h"

#include <functional>
#include <memory>

namespace lotui {

struct DialogStyle {
    Color background{0.12F, 0.15F, 0.21F, 1.0F};
    float cornerRadius{12.0F};
    EdgeInsets contentPadding{24.0F, 20.0F, 24.0F, 20.0F};
};

class Dialog final : public SingleChildWidget {
public:
    explicit Dialog(
        std::unique_ptr<Widget> content = {},
        Size preferredSize = {420.0F, 260.0F},
        DialogStyle style = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setContent(std::unique_ptr<Widget> content);
    std::unique_ptr<Widget> takeContent() noexcept;
    Widget* content() noexcept;
    const Widget* content() const noexcept;

    void setPreferredSize(Size preferredSize) noexcept;
    Size preferredSize() const noexcept;

    void setStyle(DialogStyle style) noexcept;
    const DialogStyle& style() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;

private:
    Size preferredSize_{};
    DialogStyle style_{};
};

struct DialogHostStyle {
    Color scrim{0.0F, 0.0F, 0.0F, 0.58F};
    EdgeInsets modalMargin{24.0F, 24.0F, 24.0F, 24.0F};
    bool cancelOnEscape{true};
};

enum class DialogResult {
    Accepted,
    Cancelled,
    Dismissed,
};

using DialogClosedHandler = std::function<void(DialogResult)>;

class DialogHost final : public Widget {
public:
    explicit DialogHost(
        std::unique_ptr<Widget> content,
        DialogHostStyle style = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setContent(std::unique_ptr<Widget> content);
    Widget* content() noexcept;
    const Widget* content() const noexcept;

    Widget& showModal(
        std::unique_ptr<Widget> modal,
        DialogClosedHandler onClosed = {});
    void acceptModal();
    void cancelModal();
    void dismissModal();
    std::unique_ptr<Widget> takeModal() noexcept;
    Widget* modal() noexcept;
    const Widget* modal() const noexcept;
    bool hasModal() const noexcept;

    void setStyle(DialogHostStyle style) noexcept;
    const DialogHostStyle& style() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    void paintChildren(std::vector<PaintCommand>& commands) const override;
    void collectChildHitTestEntries(
        std::vector<HitTestEntry>& entries) const override;
    void collectChildFocusTargets(
        std::vector<PointerTargetId>& targets) const override;
    Widget* findChildByPointerTarget(
        PointerTargetId target) noexcept override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool onPreviewKeyEvent(const WidgetKeyEvent& event) override;
    PointerTargetId activeFocusScopeTarget() const noexcept override;

private:
    void arrangeModal();
    void closeModal(DialogResult result);

    std::unique_ptr<Widget> content_;
    std::unique_ptr<Widget> modal_;
    mutable std::unique_ptr<Widget> dismissedModal_;
    DialogClosedHandler onModalClosed_{};
    DialogHostStyle style_{};
};

} // namespace lotui
