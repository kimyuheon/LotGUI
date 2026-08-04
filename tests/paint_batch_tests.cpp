#include "renderer/paint_batch.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "paint_batch_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

lotui::PaintCommand command(
    lotui::Rect bounds,
    lotui::Rect clip,
    lotui::TextureId texture = lotui::invalidTextureId) {
    return {bounds, clip, {}, texture, 0.0F};
}

void groupsAdjacentMatchingCommands() {
    const std::vector<lotui::PaintCommand> commands{
        command({0.0F, 0.0F, 20.0F, 20.0F},
                {0.0F, 0.0F, 100.0F, 100.0F}),
        command({30.0F, 0.0F, 20.0F, 20.0F},
                {0.0F, 0.0F, 100.0F, 100.0F}),
    };

    const auto plan = lotui::buildPaintBatchPlan(commands, 1.0F, 200, 120);
    require(plan.commandIndices.size() == 2, "both commands must be visible");
    require(plan.batches.size() == 1, "matching commands must share a batch");
    require(plan.batches[0].firstInstance == 0, "batch must start at zero");
    require(plan.batches[0].instanceCount == 2, "batch must contain two instances");
}

void splitsOnClipOrTextureChanges() {
    const std::vector<lotui::PaintCommand> commands{
        command({0.0F, 0.0F, 20.0F, 20.0F},
                {0.0F, 0.0F, 100.0F, 100.0F}),
        command({30.0F, 0.0F, 20.0F, 20.0F},
                {10.0F, 0.0F, 90.0F, 100.0F}),
        command({40.0F, 0.0F, 20.0F, 20.0F},
                {10.0F, 0.0F, 90.0F, 100.0F}, 7),
    };

    const auto plan = lotui::buildPaintBatchPlan(commands, 1.0F, 200, 120);
    require(plan.batches.size() == 3, "clip and texture changes must split batches");
    require(plan.batches[1].firstInstance == 1, "second batch instance offset is wrong");
    require(plan.batches[2].firstInstance == 2, "third batch instance offset is wrong");
}

void clampsAndScalesScissors() {
    const std::vector<lotui::PaintCommand> commands{
        command({0.0F, 0.0F, 50.0F, 50.0F},
                {-2.25F, 3.25F, 20.5F, 30.25F}),
    };

    const auto plan = lotui::buildPaintBatchPlan(commands, 2.0F, 30, 80);
    const auto& scissor = plan.batches[0].scissor;
    require(scissor.x == 0 && scissor.y == 6, "scaled scissor origin is wrong");
    require(scissor.width == 30 && scissor.height == 61,
            "scaled scissor extent is wrong");
}

void removesInvisibleCommands() {
    const std::vector<lotui::PaintCommand> commands{
        command({0.0F, 0.0F, 10.0F, 10.0F},
                {20.0F, 20.0F, 10.0F, 10.0F}),
        command({0.0F, 0.0F, 10.0F, 10.0F},
                {300.0F, 300.0F, 20.0F, 20.0F}),
    };

    const auto plan = lotui::buildPaintBatchPlan(commands, 1.0F, 100, 100);
    require(plan.commandIndices.empty(), "invisible commands must be removed");
    require(plan.batches.empty(), "invisible commands must not create batches");
}

} // namespace

int main() {
    groupsAdjacentMatchingCommands();
    splitsOnClipOrTextureChanges();
    clampsAndScalesScissors();
    removesInvisibleCommands();
    std::cout << "paint_batch_tests passed\n";
    return EXIT_SUCCESS;
}
