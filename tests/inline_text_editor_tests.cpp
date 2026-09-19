#include "core/widget_tree.h"
#include "widgets/box.h"
#include "widgets/inline_text_editor.h"
#include "widgets/list_control.h"
#include "widgets/popup.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "inline_text_editor_tests failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.01F;
}

class TestTextLayout final : public lotui::TextLayout {
public:
    explicit TestTextLayout(lotui::Size size)
        : size_(size) {
    }

    lotui::Size size() const noexcept override {
        return size_;
    }

    float baseline() const noexcept override {
        return size_.height * 0.8F;
    }

    void appendPaintCommands(
        lotui::Point origin,
        lotui::Rect clip,
        lotui::Color color,
        std::vector<lotui::PaintCommand>& commands) const override {
        commands.push_back({
            {origin.x, origin.y, size_.width, size_.height},
            clip,
            color,
            107,
            0.0F,
        });
    }

private:
    lotui::Size size_{};
};

class TestTextEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view text,
        const lotui::TextStyle& style,
        const lotui::TextLayoutOptions& options) const override {
        float width = static_cast<float>(text.size()) * style.fontSize * 0.5F;
        if (std::isfinite(options.maximumWidth)) {
            width = std::min(width, options.maximumWidth);
        }
        return std::make_unique<TestTextLayout>(
            lotui::Size{width, style.fontSize * 1.2F});
    }
};

struct HostFixture {
    std::unique_ptr<lotui::WidgetTree> tree;
    lotui::PopupHost* host{nullptr};
};

HostFixture makeHost() {
    auto host = std::make_unique<lotui::PopupHost>(
        std::make_unique<lotui::Box>(lotui::Size{400.0F, 240.0F}));
    lotui::PopupHost* observed = host.get();
    auto tree = std::make_unique<lotui::WidgetTree>(std::move(host));
    tree->layout({0.0F, 0.0F, 400.0F, 240.0F});
    return {std::move(tree), observed};
}

void commitsUtf8TextAndMatchesTheAnchor() {
    auto engine = std::make_shared<TestTextEngine>();
    HostFixture fixture = makeHost();
    std::optional<std::string> committed;
    lotui::InlineTextEditHandlers handlers;
    handlers.committed = [&committed](std::string text) {
        committed = std::move(text);
    };
    const lotui::Rect anchor{42.0F, 55.0F, 160.0F, 28.0F};
    lotui::InlineTextEditor::showAt(
        *fixture.host, engine, anchor, "이름", std::move(handlers));
    fixture.tree->layout({0.0F, 0.0F, 400.0F, 240.0F});

    const lotui::Rect popup = fixture.host->popupBounds();
    require(near(popup.x, anchor.x) && near(popup.y, anchor.y) &&
            near(popup.width, anchor.width) &&
            near(popup.height, anchor.height),
        "an inline editor must overlay the exact cell bounds");
    require(fixture.tree->textInputState().enabled,
        "an inline editor must immediately own text and IME focus");
    fixture.tree->textInput({
        lotui::TextInputEventType::Composition, "한", 0, 0});
    require(!committed,
        "IME composition must not commit an inline edit");
    fixture.tree->textInput({
        lotui::TextInputEventType::Commit, "한글"});
    fixture.tree->textInput({lotui::TextInputEventType::CompositionEnd});
    fixture.tree->keyPressed(lotui::KeyCode::Enter);
    require(committed == "이름한글" && !fixture.host->hasPopup(),
        "Enter must commit the final UTF-8 text and close the editor");
}

void validatesBeforeEnterCommit() {
    auto engine = std::make_shared<TestTextEngine>();
    HostFixture fixture = makeHost();
    std::optional<std::string> committed;
    std::vector<std::string> errors;
    lotui::InlineTextEditHandlers handlers;
    handlers.committed = [&committed](std::string text) {
        committed = std::move(text);
    };
    handlers.validate = [](std::string_view text)
        -> std::optional<std::string> {
        return text.empty()
            ? std::optional<std::string>{"required"}
            : std::nullopt;
    };
    handlers.validationFailed = [&errors](std::string message) {
        errors.push_back(std::move(message));
    };
    lotui::InlineTextEditor::showAt(
        *fixture.host,
        engine,
        {20.0F, 30.0F, 150.0F, 30.0F},
        {},
        std::move(handlers));
    fixture.tree->layout({0.0F, 0.0F, 400.0F, 240.0F});

    fixture.tree->keyPressed(lotui::KeyCode::Enter);
    require(fixture.host->hasPopup() && !committed &&
            errors.size() == 1 && errors.front() == "required",
        "invalid Enter submission must keep the editor open and report error");
    fixture.tree->textInput({
        lotui::TextInputEventType::Commit, "정상 값"});
    fixture.tree->keyPressed(lotui::KeyCode::Enter);
    require(committed == "정상 값" && !fixture.host->hasPopup(),
        "corrected text must pass validation and commit");
}

void handlesEscapeAndFocusLoss() {
    auto engine = std::make_shared<TestTextEngine>();
    HostFixture fixture = makeHost();
    std::vector<lotui::PopupCloseReason> cancellations;
    std::vector<std::string> commits;

    lotui::InlineTextEditHandlers escapeHandlers;
    escapeHandlers.committed = [&commits](std::string text) {
        commits.push_back(std::move(text));
    };
    escapeHandlers.cancelled = [&cancellations](
        lotui::PopupCloseReason reason) {
        cancellations.push_back(reason);
    };
    lotui::InlineTextEditor::showAt(
        *fixture.host,
        engine,
        {20.0F, 30.0F, 150.0F, 30.0F},
        "cancel me",
        std::move(escapeHandlers));
    fixture.tree->layout({0.0F, 0.0F, 400.0F, 240.0F});
    fixture.tree->keyPressed(lotui::KeyCode::Escape);
    require(cancellations.size() == 1 &&
            cancellations.front() == lotui::PopupCloseReason::Escape &&
            commits.empty(),
        "Escape must cancel without changing the model");

    lotui::InlineTextEditHandlers focusLossHandlers;
    focusLossHandlers.committed = [&commits](std::string text) {
        commits.push_back(std::move(text));
    };
    lotui::InlineTextEditor::showAt(
        *fixture.host,
        engine,
        {20.0F, 30.0F, 150.0F, 30.0F},
        "focus value",
        std::move(focusLossHandlers));
    fixture.tree->layout({0.0F, 0.0F, 400.0F, 240.0F});
    fixture.tree->pointerPressed(
        {300.0F, 180.0F}, lotui::PointerButton::Primary);
    fixture.tree->pointerReleased(
        {300.0F, 180.0F}, lotui::PointerButton::Primary);
    require(commits.size() == 1 && commits.front() == "focus value" &&
            !fixture.host->hasPopup(),
        "an outside click must commit a valid edit as focus-loss handling");
}

void editsAListControlTextCell() {
    auto engine = std::make_shared<TestTextEngine>();
    lotui::ListCell name;
    name.text = "기존";

    auto hostPointer = std::make_shared<lotui::PopupHost*>(nullptr);
    auto listPointer = std::make_shared<lotui::ListControl*>(nullptr);
    auto list = std::make_unique<lotui::ListControl>(
        engine,
        std::vector<lotui::ListColumn>{
            {"name", "Name", 180.0F, 80.0F, true}},
        std::vector<lotui::ListRow>{{name}},
        lotui::Size{220.0F, 120.0F},
        lotui::ListControl::SelectionChangedHandler{},
        [engine, hostPointer, listPointer](
            const lotui::ListCellEvent& event) {
            if (event.action != lotui::ListCellAction::BeginEdit ||
                *hostPointer == nullptr || *listPointer == nullptr) {
                return;
            }
            const lotui::ListCell* value =
                (*listPointer)->cell(event.address);
            lotui::InlineTextEditHandlers handlers;
            handlers.committed =
                [listPointer, address = event.address](std::string text) {
                    (*listPointer)->setCellText(address, std::move(text));
                };
            lotui::InlineTextEditor::showAt(
                **hostPointer,
                engine,
                event.anchor,
                value->text,
                std::move(handlers));
        });
    *listPointer = list.get();
    auto host = std::make_unique<lotui::PopupHost>(std::move(list));
    *hostPointer = host.get();

    lotui::WidgetTree tree(std::move(host));
    tree.layout({0.0F, 0.0F, 220.0F, 140.0F});
    tree.keyPressed(lotui::KeyCode::Tab);
    tree.keyPressed(lotui::KeyCode::Down);
    tree.keyPressed(lotui::KeyCode::Enter);
    require((*hostPointer)->hasPopup() && tree.textInputState().enabled,
        "Enter on a selected text cell must start immediate inline editing");
    tree.textInput({lotui::TextInputEventType::Commit, " 한글"});
    tree.keyPressed(lotui::KeyCode::Enter);
    require((*listPointer)->cell({0, 0})->text == "기존 한글" &&
            !(*hostPointer)->hasPopup(),
        "committing the editor must update the retained list cell model");
}

} // namespace

int main() {
    commitsUtf8TextAndMatchesTheAnchor();
    validatesBeforeEnterCommit();
    handlesEscapeAndFocusLoss();
    editsAListControlTextCell();
    std::cout << "inline_text_editor_tests passed\n";
    return EXIT_SUCCESS;
}
