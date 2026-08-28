#pragma once

#include "widgets/single_child_widget.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace lotui {

struct DialogStyle {
    Color background{0.12F, 0.15F, 0.21F, 1.0F};
    float cornerRadius{12.0F};
    EdgeInsets titlePadding{24.0F, 16.0F, 24.0F, 12.0F};
    Color titleDivider{0.30F, 0.34F, 0.42F, 1.0F};
    float titleDividerHeight{1.0F};
    float titleContentSpacing{8.0F};
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

    void setTitle(std::unique_ptr<Widget> title);
    std::unique_ptr<Widget> takeTitle() noexcept;
    Widget* title() noexcept;
    const Widget* title() const noexcept;

    void setPreferredSize(Size preferredSize) noexcept;
    Size preferredSize() const noexcept;

    void setStyle(DialogStyle style) noexcept;
    const DialogStyle& style() const noexcept;

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

private:
    std::unique_ptr<Widget> title_;
    Size preferredSize_{};
    DialogStyle style_{};
};

struct DialogHostStyle {
    Color scrim{0.0F, 0.0F, 0.0F, 0.58F};
    EdgeInsets modalMargin{24.0F, 24.0F, 24.0F, 24.0F};
    bool cancelOnEscape{true};
    bool acceptOnUnhandledEnter{false};
};

enum class DialogResult {
    Accepted,
    Cancelled,
    Dismissed,
};

using DialogClosedHandler = std::function<void(DialogResult)>;
using ModelessDialogId = std::uint64_t;
inline constexpr ModelessDialogId invalidModelessDialogId = 0;
using ModelessClosedHandler = std::function<void()>;

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

    ModelessDialogId showModeless(
        std::unique_ptr<Widget> dialog,
        Rect bounds = {80.0F, 80.0F, 420.0F, 260.0F},
        ModelessClosedHandler onClosed = {});
    bool closeModeless(ModelessDialogId id);
    bool bringModelessToFront(ModelessDialogId id);
    bool setModelessBounds(ModelessDialogId id, Rect bounds);
    Widget* modeless(ModelessDialogId id) noexcept;
    const Widget* modeless(ModelessDialogId id) const noexcept;
    std::size_t modelessCount() const noexcept;

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
    void onPreviewPointerEvent(
        PointerTargetId target,
        const WidgetPointerEvent& event) override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool onPreviewKeyEvent(const WidgetKeyEvent& event) override;
    bool onUnhandledKeyEvent(const WidgetKeyEvent& event) override;
    PointerTargetId activeFocusScopeTarget() const noexcept override;

private:
    struct ModelessEntry {
        ModelessDialogId id{invalidModelessDialogId};
        Rect bounds{};
        std::unique_ptr<Widget> dialog;
        ModelessClosedHandler onClosed{};
    };

    void arrangeModal();
    void arrangeModeless(ModelessEntry& entry);
    void closeModal(DialogResult result);

    std::unique_ptr<Widget> content_;
    std::unique_ptr<Widget> modal_;
    mutable std::unique_ptr<Widget> dismissedModal_;
    std::vector<ModelessEntry> modelessDialogs_;
    mutable std::vector<std::unique_ptr<Widget>> dismissedModelessDialogs_;
    ModelessDialogId nextModelessId_{1};
    DialogClosedHandler onModalClosed_{};
    DialogHostStyle style_{};
};

} // namespace lotui
