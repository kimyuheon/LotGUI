#pragma once

#include "core/widget.h"

#include <memory>

namespace lotui {

class SingleChildWidget : public Widget {
public:
    void setChild(std::unique_ptr<Widget> child);
    std::unique_ptr<Widget> takeChild() noexcept;

    Widget* child() noexcept;
    const Widget* child() const noexcept;

protected:
    explicit SingleChildWidget(std::unique_ptr<Widget> child = {});

    void paintChildren(std::vector<PaintCommand>& commands) const override;
    void collectChildHitTestEntries(
        std::vector<HitTestEntry>& entries) const override;
    void collectChildFocusTargets(
        std::vector<PointerTargetId>& targets) const override;
    Widget* findChildByPointerTarget(
        PointerTargetId target) noexcept override;

private:
    std::unique_ptr<Widget> child_;
};

} // namespace lotui
