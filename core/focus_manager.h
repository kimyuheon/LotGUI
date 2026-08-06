#pragma once

#include "core/pointer_router.h"

#include <vector>

namespace lotui {

struct FocusChange {
    PointerTargetId previous{invalidPointerTarget};
    PointerTargetId current{invalidPointerTarget};

    bool changed() const noexcept { return previous != current; }
};

class FocusManager {
public:
    FocusChange setTargets(std::vector<PointerTargetId> targets);
    FocusChange focus(PointerTargetId target) noexcept;
    FocusChange moveFocus(bool reverse) noexcept;
    FocusChange clear() noexcept;

    PointerTargetId focusedTarget() const noexcept;
    const std::vector<PointerTargetId>& targets() const noexcept;

private:
    std::vector<PointerTargetId> targets_;
    PointerTargetId focusedTarget_{invalidPointerTarget};
};

} // namespace lotui
