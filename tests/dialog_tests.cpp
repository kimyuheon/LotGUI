#include "core/widget_tree.h"
#include "widgets/box.h"
#include "widgets/button.h"
#include "widgets/dialog.h"
#include "widgets/linear_layout.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "dialog_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.01F;
}

void centersAndPaintsModalAboveTheScrim() {
    auto content = std::make_unique<lotui::Box>(
        lotui::Size{800.0F, 600.0F},
        lotui::Color{0.1F, 0.2F, 0.3F, 1.0F});
    auto modalContent = std::make_unique<lotui::Box>(
        lotui::Size{100.0F, 60.0F},
        lotui::Color{0.8F, 0.2F, 0.1F, 1.0F});
    auto modal = std::make_unique<lotui::Dialog>(
        std::move(modalContent), lotui::Size{320.0F, 180.0F});
    lotui::Dialog* observedModal = modal.get();

    auto host = std::make_unique<lotui::DialogHost>(std::move(content));
    host->showModal(std::move(modal));
    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 800.0F, 600.0F});

    require(near(observedModal->bounds().x, 240.0F) &&
            near(observedModal->bounds().y, 210.0F) &&
            near(observedModal->bounds().width, 320.0F) &&
            near(observedModal->bounds().height, 180.0F),
        "the modal must be centered at its measured size");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() == 4,
        "content, scrim, dialog, and dialog content must all paint");
    require(near(commands[0].color.blue, 0.3F) &&
            near(commands[1].color.alpha, 0.58F) &&
            near(commands[2].cornerRadius, 12.0F) &&
            near(commands[3].color.red, 0.8F),
        "the scrim and modal must paint after the application content");
}

void laysOutAndPaintsAnOptionalTitleArea() {
    auto content = std::make_unique<lotui::Box>(
        lotui::Size{120.0F, 60.0F},
        lotui::Color{0.8F, 0.2F, 0.1F, 1.0F});
    auto title = std::make_unique<lotui::Box>(
        lotui::Size{100.0F, 24.0F},
        lotui::Color{0.2F, 0.7F, 0.9F, 1.0F});
    lotui::Box* observedTitle = title.get();
    lotui::Dialog dialog(
        std::move(content), lotui::Size{320.0F, 200.0F});
    dialog.setTitle(std::move(title));
    dialog.arrange(
        {10.0F, 20.0F, 320.0F, 200.0F},
        {0.0F, 0.0F, 400.0F, 300.0F});

    require(near(observedTitle->bounds().x, 34.0F) &&
            near(observedTitle->bounds().y, 36.0F) &&
            near(observedTitle->bounds().width, 100.0F) &&
            near(observedTitle->bounds().height, 24.0F),
        "the optional title widget must use the configured title padding");
    require(dialog.content()->bounds().y >
            observedTitle->bounds().y + observedTitle->bounds().height,
        "dialog content must be arranged below the title and divider");

    std::vector<lotui::PaintCommand> commands;
    dialog.paint(commands);
    require(commands.size() == 4 &&
            near(commands[1].bounds.height, 1.0F) &&
            near(commands[2].color.blue, 0.9F) &&
            near(commands[3].color.red, 0.8F),
        "the dialog must paint its background, divider, title, and content");
}

void blocksBackgroundPointerInput() {
    int backgroundClicks = 0;
    int modalClicks = 0;
    auto background = std::make_unique<lotui::Button>(
        lotui::Size{800.0F, 600.0F},
        [&backgroundClicks]() { ++backgroundClicks; });
    auto modalButton = std::make_unique<lotui::Button>(
        lotui::Size{200.0F, 80.0F},
        [&modalClicks]() { ++modalClicks; });
    auto modal = std::make_unique<lotui::Dialog>(
        std::move(modalButton), lotui::Size{300.0F, 180.0F});

    auto host = std::make_unique<lotui::DialogHost>(std::move(background));
    host->showModal(std::move(modal));
    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 800.0F, 600.0F});

    tree.pointerPressed({40.0F, 40.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({40.0F, 40.0F}, lotui::PointerButton::Primary);
    require(backgroundClicks == 0 && modalClicks == 0,
        "the modal scrim must consume clicks outside the dialog");

    tree.pointerPressed({400.0F, 300.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({400.0F, 300.0F}, lotui::PointerButton::Primary);
    require(backgroundClicks == 0 && modalClicks == 1,
        "input inside the dialog must reach the modal widget");
}

void trapsAndRestoresKeyboardFocus() {
    auto background = std::make_unique<lotui::Row>();
    auto firstBackground = std::make_unique<lotui::Button>();
    auto secondBackground = std::make_unique<lotui::Button>();
    lotui::Button* firstBackgroundButton = firstBackground.get();
    lotui::Button* secondBackgroundButton = secondBackground.get();
    background->addChild(std::move(firstBackground));
    background->addChild(std::move(secondBackground));

    auto host = std::make_unique<lotui::DialogHost>(std::move(background));
    lotui::DialogHost* observedHost = host.get();
    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});

    tree.keyPressed(lotui::KeyCode::Tab);
    tree.keyPressed(lotui::KeyCode::Tab);
    require(secondBackgroundButton->isFocused(),
        "the setup must focus the second background control");

    auto modalControls = std::make_unique<lotui::Row>();
    auto firstModal = std::make_unique<lotui::Button>();
    auto secondModal = std::make_unique<lotui::Button>();
    lotui::Button* firstModalButton = firstModal.get();
    lotui::Button* secondModalButton = secondModal.get();
    modalControls->addChild(std::move(firstModal));
    modalControls->addChild(std::move(secondModal));
    observedHost->showModal(std::make_unique<lotui::Dialog>(
        std::move(modalControls), lotui::Size{320.0F, 160.0F}));
    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});

    require(firstModalButton->isFocused() &&
            !firstBackgroundButton->isFocused() &&
            !secondBackgroundButton->isFocused(),
        "opening a modal must move focus into the dialog");
    tree.keyPressed(lotui::KeyCode::Tab);
    require(secondModalButton->isFocused(),
        "Tab traversal must stay inside the modal focus scope");
    tree.keyPressed(lotui::KeyCode::Tab);
    require(firstModalButton->isFocused(),
        "modal focus traversal must wrap without reaching the background");

    std::unique_ptr<lotui::Widget> closed = observedHost->takeModal();
    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});
    require(secondBackgroundButton->isFocused(),
        "closing a modal must restore the previous background focus");
}

void dismissesSafelyFromAButtonCallback() {
    auto host = std::make_unique<lotui::DialogHost>(
        std::make_unique<lotui::Box>());
    lotui::DialogHost* observedHost = host.get();
    auto closeButton = std::make_unique<lotui::Button>(
        lotui::Size{180.0F, 60.0F},
        [observedHost]() { observedHost->dismissModal(); });
    observedHost->showModal(std::make_unique<lotui::Dialog>(
        std::move(closeButton), lotui::Size{280.0F, 140.0F}));

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});
    tree.pointerPressed({300.0F, 150.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({300.0F, 150.0F}, lotui::PointerButton::Primary);
    require(!observedHost->hasModal(),
        "a dialog button must be able to dismiss its own modal safely");

    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});
    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() == 1,
        "a dismissed modal and its scrim must stop painting immediately");
}

void reportsResultsAndRejectsNestedModals() {
    auto host = std::make_unique<lotui::DialogHost>(
        std::make_unique<lotui::Box>());
    lotui::DialogHost* observedHost = host.get();
    std::vector<lotui::DialogResult> results;

    observedHost->showModal(
        std::make_unique<lotui::Box>(),
        [&results](lotui::DialogResult result) {
            results.push_back(result);
        });
    try {
        observedHost->showModal(std::make_unique<lotui::Box>());
        require(false, "a second active modal must be rejected");
    } catch (const std::logic_error&) {
    }
    observedHost->acceptModal();

    observedHost->showModal(
        std::make_unique<lotui::Box>(),
        [&results](lotui::DialogResult result) {
            results.push_back(result);
        });
    observedHost->cancelModal();

    observedHost->showModal(
        std::make_unique<lotui::Box>(),
        [&results](lotui::DialogResult result) {
            results.push_back(result);
        });
    observedHost->dismissModal();
    observedHost->dismissModal();

    require(results.size() == 3 &&
            results[0] == lotui::DialogResult::Accepted &&
            results[1] == lotui::DialogResult::Cancelled &&
            results[2] == lotui::DialogResult::Dismissed,
        "each close path must report exactly one distinct result");
}

void cancelsWithEscapeAndRestoresFocus() {
    auto background = std::make_unique<lotui::Button>();
    lotui::Button* backgroundButton = background.get();
    auto host = std::make_unique<lotui::DialogHost>(std::move(background));
    lotui::DialogHost* observedHost = host.get();
    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});
    tree.keyPressed(lotui::KeyCode::Tab);
    require(backgroundButton->isFocused(),
        "the setup must focus the background control");

    lotui::DialogResult result = lotui::DialogResult::Dismissed;
    observedHost->showModal(
        std::make_unique<lotui::Dialog>(
            std::make_unique<lotui::Button>(),
            lotui::Size{280.0F, 140.0F}),
        [&result](lotui::DialogResult closedResult) {
            result = closedResult;
        });
    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});

    const auto escape = tree.keyPressed(lotui::KeyCode::Escape);
    require(escape.handled && escape.focusChanged &&
            !observedHost->hasModal() &&
            result == lotui::DialogResult::Cancelled &&
            backgroundButton->isFocused(),
        "Escape must cancel the modal and restore background focus");

    lotui::DialogHostStyle style = observedHost->style();
    style.cancelOnEscape = false;
    observedHost->setStyle(style);
    observedHost->showModal(std::make_unique<lotui::Dialog>(
        std::make_unique<lotui::Button>(),
        lotui::Size{280.0F, 140.0F}));
    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});
    require(!tree.keyPressed(lotui::KeyCode::Escape).handled &&
            observedHost->hasModal(),
        "Escape cancellation must be configurable for required dialogs");
}

void acceptsOnlyUnhandledEnterAsTheDefaultAction() {
    lotui::DialogHostStyle style;
    style.acceptOnUnhandledEnter = true;
    auto host = std::make_unique<lotui::DialogHost>(
        std::make_unique<lotui::Box>(), style);
    lotui::DialogHost* observedHost = host.get();
    lotui::DialogResult result = lotui::DialogResult::Dismissed;
    observedHost->showModal(
        std::make_unique<lotui::Dialog>(
            std::make_unique<lotui::Box>(),
            lotui::Size{280.0F, 140.0F}),
        [&result](lotui::DialogResult closedResult) {
            result = closedResult;
        });
    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});

    require(tree.keyPressed(lotui::KeyCode::Enter).handled &&
            result == lotui::DialogResult::Accepted &&
            !observedHost->hasModal(),
        "an unhandled Enter must accept when the option is enabled");

    int buttonClicks = 0;
    result = lotui::DialogResult::Dismissed;
    auto focusedButton = std::make_unique<lotui::Button>(
        lotui::Size{160.0F, 60.0F},
        [&buttonClicks]() { ++buttonClicks; });
    observedHost->showModal(
        std::make_unique<lotui::Dialog>(
            std::move(focusedButton), lotui::Size{280.0F, 140.0F}),
        [&result](lotui::DialogResult closedResult) {
            result = closedResult;
        });
    tree.layout({0.0F, 0.0F, 600.0F, 300.0F});
    tree.keyPressed(lotui::KeyCode::Enter);
    require(observedHost->hasModal() &&
            result == lotui::DialogResult::Dismissed,
        "a focused widget that handles Enter must take precedence");
    tree.keyReleased(lotui::KeyCode::Enter);
    require(buttonClicks == 1 && observedHost->hasModal(),
        "the focused button must retain its normal Enter activation");
}

void managesModelessZOrderAndBounds() {
    int backgroundClicks = 0;
    int firstClicks = 0;
    int secondClicks = 0;
    auto background = std::make_unique<lotui::Button>(
        lotui::Size{600.0F, 400.0F},
        [&backgroundClicks]() { ++backgroundClicks; });
    auto host = std::make_unique<lotui::DialogHost>(std::move(background));
    lotui::DialogHost* observedHost = host.get();

    const auto firstId = observedHost->showModeless(
        std::make_unique<lotui::Dialog>(
            std::make_unique<lotui::Button>(
                lotui::Size{160.0F, 60.0F},
                [&firstClicks]() { ++firstClicks; }),
            lotui::Size{240.0F, 160.0F}),
        {80.0F, 80.0F, 240.0F, 160.0F});
    const auto secondId = observedHost->showModeless(
        std::make_unique<lotui::Dialog>(
            std::make_unique<lotui::Button>(
                lotui::Size{160.0F, 60.0F},
                [&secondClicks]() { ++secondClicks; }),
            lotui::Size{240.0F, 160.0F}),
        {140.0F, 100.0F, 240.0F, 160.0F});

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 600.0F, 400.0F});
    require(observedHost->modelessCount() == 2 &&
            near(observedHost->modeless(firstId)->bounds().x, 80.0F) &&
            near(observedHost->modeless(secondId)->bounds().x, 140.0F),
        "modeless dialogs must retain their IDs and requested bounds");

    tree.pointerPressed({200.0F, 150.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({200.0F, 150.0F}, lotui::PointerButton::Primary);
    require(secondClicks == 1 && firstClicks == 0,
        "the most recently shown overlapping dialog must be on top");

    require(observedHost->bringModelessToFront(firstId),
        "an existing modeless dialog must move to the front");
    tree.pointerPressed({200.0F, 150.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({200.0F, 150.0F}, lotui::PointerButton::Primary);
    require(firstClicks == 1 && secondClicks == 1,
        "hit testing must follow the updated modeless Z-order");

    tree.pointerPressed({90.0F, 90.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({90.0F, 90.0F}, lotui::PointerButton::Primary);
    require(backgroundClicks == 0,
        "the modeless dialog surface must prevent click-through");

    require(observedHost->setModelessBounds(
                secondId, {20.0F, 30.0F, 260.0F, 180.0F}) &&
            near(observedHost->modeless(secondId)->bounds().x, 20.0F) &&
            near(observedHost->modeless(secondId)->bounds().width, 260.0F),
        "modeless bounds updates must arrange the dialog immediately");
}

void closesModelessSafelyFromItsButton() {
    auto host = std::make_unique<lotui::DialogHost>(
        std::make_unique<lotui::Box>());
    lotui::DialogHost* observedHost = host.get();
    lotui::ModelessDialogId id = lotui::invalidModelessDialogId;
    int closed = 0;
    auto closeButton = std::make_unique<lotui::Button>(
        lotui::Size{160.0F, 60.0F},
        [&]() { observedHost->closeModeless(id); });
    id = observedHost->showModeless(
        std::make_unique<lotui::Dialog>(
            std::move(closeButton), lotui::Size{240.0F, 140.0F}),
        {100.0F, 80.0F, 240.0F, 140.0F},
        [&closed]() { ++closed; });

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 500.0F, 300.0F});
    tree.pointerPressed({220.0F, 150.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({220.0F, 150.0F}, lotui::PointerButton::Primary);
    require(closed == 1 && observedHost->modelessCount() == 0 &&
            observedHost->modeless(id) == nullptr,
        "a modeless child callback must close its own window exactly once");
    require(!observedHost->closeModeless(id),
        "closing an unknown modeless ID must report false");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() == 1,
        "a closed modeless dialog must stop painting immediately");
}

void modalInputTemporarilyBlocksModelessDialogs() {
    int modelessClicks = 0;
    auto host = std::make_unique<lotui::DialogHost>(
        std::make_unique<lotui::Box>());
    lotui::DialogHost* observedHost = host.get();
    observedHost->showModeless(
        std::make_unique<lotui::Dialog>(
            std::make_unique<lotui::Button>(
                lotui::Size{120.0F, 50.0F},
                [&modelessClicks]() { ++modelessClicks; }),
            lotui::Size{200.0F, 120.0F}),
        {20.0F, 20.0F, 200.0F, 120.0F});
    observedHost->showModal(std::make_unique<lotui::Dialog>(
        std::make_unique<lotui::Box>(), lotui::Size{260.0F, 140.0F}));

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 600.0F, 400.0F});
    tree.pointerPressed({80.0F, 70.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({80.0F, 70.0F}, lotui::PointerButton::Primary);
    require(modelessClicks == 0,
        "an active modal must block input to every modeless dialog");

    observedHost->cancelModal();
    tree.layout({0.0F, 0.0F, 600.0F, 400.0F});
    tree.pointerPressed({80.0F, 70.0F}, lotui::PointerButton::Primary);
    tree.pointerReleased({80.0F, 70.0F}, lotui::PointerButton::Primary);
    require(modelessClicks == 1,
        "modeless input must resume after the modal closes");
}

} // namespace

int main() {
    centersAndPaintsModalAboveTheScrim();
    laysOutAndPaintsAnOptionalTitleArea();
    blocksBackgroundPointerInput();
    trapsAndRestoresKeyboardFocus();
    dismissesSafelyFromAButtonCallback();
    reportsResultsAndRejectsNestedModals();
    cancelsWithEscapeAndRestoresFocus();
    acceptsOnlyUnhandledEnterAsTheDefaultAction();
    managesModelessZOrderAndBounds();
    closesModelessSafelyFromItsButton();
    modalInputTemporarilyBlocksModelessDialogs();
    std::cout << "dialog_tests passed\n";
    return EXIT_SUCCESS;
}
