#include "core/layout.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "layout_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.01F;
}

void distributesRowFlex() {
    const std::vector<lotui::LayoutItem> children{
        {{40.0F, 20.0F}, {40.0F, 20.0F}, {40.0F, 20.0F}, 0.0F},
        {{20.0F, 20.0F}, {10.0F, 20.0F}, {200.0F, 20.0F}, 1.0F},
        {{20.0F, 20.0F}, {10.0F, 20.0F}, {200.0F, 20.0F}, 2.0F},
    };
    lotui::LinearLayoutOptions options;
    options.spacing = 10.0F;
    options.crossAxisAlignment = lotui::CrossAxisAlignment::Stretch;

    const auto result = lotui::layoutRow(
        lotui::LayoutConstraints::tight({200.0F, 40.0F}),
        children,
        options);
    require(near(result.children[0].width, 40.0F),
            "fixed child width changed");
    require(near(result.children[1].width, 53.333F),
            "first flex share is wrong");
    require(near(result.children[2].width, 86.667F),
            "second flex share is wrong");
    require(near(result.children[2].x + result.children[2].width, 200.0F),
            "row does not fill its tight constraint");
    require(near(result.children[1].height, 20.0F),
            "child maximum must limit cross-axis stretch");
}

void honorsFlexMaximum() {
    const std::vector<lotui::LayoutItem> children{
        {{20.0F, 10.0F}, {}, {40.0F, 10.0F}, 1.0F},
        {{20.0F, 10.0F}, {}, {200.0F, 10.0F}, 1.0F},
    };
    const auto result = lotui::layoutRow(
        lotui::LayoutConstraints::tight({120.0F, 10.0F}), children);
    require(near(result.children[0].width, 40.0F),
            "capped flex child exceeded maximum");
    require(near(result.children[1].width, 80.0F),
            "remaining flex space was not redistributed");
}

void alignsSpaceBetween() {
    const std::vector<lotui::LayoutItem> children{
        {{20.0F, 10.0F}, {}, {20.0F, 10.0F}, 0.0F},
        {{20.0F, 10.0F}, {}, {20.0F, 10.0F}, 0.0F},
    };
    lotui::LinearLayoutOptions options;
    options.mainAxisAlignment = lotui::MainAxisAlignment::SpaceBetween;
    const auto result = lotui::layoutRow(
        lotui::LayoutConstraints::tight({100.0F, 10.0F}),
        children,
        options);
    require(near(result.children[0].x, 0.0F),
            "space-between first child is misplaced");
    require(near(result.children[1].x, 80.0F),
            "space-between last child is misplaced");
}

void laysOutPaddedColumn() {
    const std::vector<lotui::LayoutItem> children{
        {{20.0F, 30.0F}, {}, {20.0F, 30.0F}, 0.0F},
        {{40.0F, 20.0F}, {}, {40.0F, 20.0F}, 0.0F},
    };
    lotui::LinearLayoutOptions options;
    options.spacing = 5.0F;
    options.padding = {10.0F, 8.0F, 10.0F, 8.0F};
    options.mainAxisSize = lotui::MainAxisSize::Min;
    options.crossAxisAlignment = lotui::CrossAxisAlignment::Center;

    const auto result = lotui::layoutColumn(
        lotui::LayoutConstraints::loose({100.0F, 100.0F}),
        children,
        options);
    require(near(result.size.width, 60.0F) && near(result.size.height, 71.0F),
            "column shrink-wrap size is wrong");
    require(near(result.children[0].x, 20.0F) &&
                near(result.children[0].y, 8.0F),
            "centered first column child is misplaced");
    require(near(result.children[1].x, 10.0F) &&
                near(result.children[1].y, 43.0F),
            "second column child is misplaced");
}

void shrinksChildrenToFit() {
    const std::vector<lotui::LayoutItem> children{
        {{60.0F, 10.0F}, {20.0F, 10.0F}, {80.0F, 10.0F}, 0.0F},
        {{60.0F, 10.0F}, {20.0F, 10.0F}, {80.0F, 10.0F}, 0.0F},
    };
    lotui::LinearLayoutOptions options;
    options.spacing = 10.0F;
    const auto result = lotui::layoutRow(
        lotui::LayoutConstraints::tight({100.0F, 10.0F}),
        children,
        options);
    require(near(result.children[0].width, 45.0F) &&
                near(result.children[1].width, 45.0F),
            "overflowing children were not shrunk evenly");
}

} // namespace

int main() {
    distributesRowFlex();
    honorsFlexMaximum();
    alignsSpaceBetween();
    laysOutPaddedColumn();
    shrinksChildrenToFit();
    std::cout << "layout_tests passed\n";
    return EXIT_SUCCESS;
}
