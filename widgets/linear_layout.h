#pragma once

#include "core/widget.h"

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace lotui {

struct ChildLayout {
    float flex{0.0F};
    Size minimum{};
    Size maximum{unboundedLayoutSize, unboundedLayoutSize};
};

struct BoxDecoration {
    Color color{};
    float cornerRadius{0.0F};
};

class LinearLayout : public Widget {
public:
    Widget& addChild(
        std::unique_ptr<Widget> child,
        ChildLayout layout = {});

    template<typename T, typename... Arguments>
    T& emplaceChild(ChildLayout layout, Arguments&&... arguments) {
        static_assert(std::is_base_of<Widget, T>::value,
            "T must derive from lotui::Widget");
        auto child = std::make_unique<T>(
            std::forward<Arguments>(arguments)...);
        T& result = *child;
        addChild(std::move(child), layout);
        return result;
    }

    std::size_t childCount() const noexcept;
    Widget& childAt(std::size_t index);
    const Widget& childAt(std::size_t index) const;

    void setOptions(LinearLayoutOptions options);
    const LinearLayoutOptions& options() const noexcept;

    void setDecoration(std::optional<BoxDecoration> decoration) noexcept;
    const std::optional<BoxDecoration>& decoration() const noexcept;

    Size measure(const LayoutConstraints& constraints) const override;

protected:
    explicit LinearLayout(bool horizontal);

    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    void paintChildren(std::vector<PaintCommand>& commands) const override;
    void collectChildHitTestEntries(
        std::vector<HitTestEntry>& entries) const override;
    void collectChildFocusTargets(
        std::vector<PointerTargetId>& targets) const override;
    Widget* findChildByPointerTarget(
        PointerTargetId target) noexcept override;

private:
    struct Slot {
        std::unique_ptr<Widget> widget;
        ChildLayout layout;
    };

    std::vector<LayoutItem> makeLayoutItems(
        const LayoutConstraints& constraints) const;
    LayoutResult performLayout(
        const LayoutConstraints& constraints) const;

    bool horizontal_{false};
    LinearLayoutOptions options_{};
    std::optional<BoxDecoration> decoration_{};
    std::vector<Slot> children_;
};

class Row final : public LinearLayout {
public:
    Row();
};

class Column final : public LinearLayout {
public:
    Column();
};

} // namespace lotui
