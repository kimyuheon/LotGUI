#include "core/widget_tree.h"
#include "widgets/code_editor.h"

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
        std::cerr << "code_editor_tests: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class TestLayout final : public lotui::TextLayout {
public:
    explicit TestLayout(lotui::Size size) : size_(size) {}
    lotui::Size size() const noexcept override { return size_; }
    float baseline() const noexcept override { return size_.height * 0.8F; }
    void appendPaintCommands(lotui::Point origin, lotui::Rect clip,
        lotui::Color color,
        std::vector<lotui::PaintCommand>& commands) const override {
        if (size_.width > 0.0F) commands.push_back({
            {origin.x, origin.y, size_.width, size_.height},
            clip, color, 41, 0.0F});
    }
private:
    lotui::Size size_;
};

class TestEngine final : public lotui::TextEngine {
public:
    std::unique_ptr<lotui::TextLayout> createLayout(
        std::string_view value, const lotui::TextStyle&,
        const lotui::TextLayoutOptions&) const override {
        return std::make_unique<TestLayout>(lotui::Size{
            static_cast<float>(value.size()) * 7.0F, 16.0F});
    }
};

void editsUtf8AndHistory() {
    auto engine = std::make_shared<TestEngine>();
    int changes = 0;
    auto widget = std::make_unique<lotui::CodeEditor>(engine,
        "    int value = 1;\nreturn value;", "cpp",
        lotui::Size{420.0F, 180.0F},
        [&changes](const std::string&) { ++changes; });
    auto* editor = widget.get();
    lotui::WidgetTree tree(std::move(widget));
    tree.layout({0.0F, 0.0F, 420.0F, 180.0F});
    tree.keyPressed(lotui::KeyCode::Tab);
    require(editor->isFocused() && tree.textInputState().enabled,
        "editor should accept native text input");
    require(!tree.keyPressed(lotui::KeyCode::F2).handled &&
            !tree.keyReleased(lotui::KeyCode::F2).handled &&
            !tree.keyPressed(lotui::KeyCode::Tab, {false, true}).handled,
        "unhandled keys should remain available to the host application");
    tree.keyPressed(lotui::KeyCode::End);
    tree.keyPressed(lotui::KeyCode::Enter);
    require(editor->text().find("1;\n    \nreturn") != std::string::npos &&
        editor->lineCount() == 3, "Enter should retain leading indentation");
    tree.textInput({lotui::TextInputEventType::Composition, "한"});
    require(editor->composition() == "한" &&
        editor->text().find("한") == std::string::npos,
        "IME preedit should not change committed text");
    tree.textInput({lotui::TextInputEventType::Commit, "한"});
    tree.textInput({lotui::TextInputEventType::CompositionEnd});
    require(editor->text().find("\n    한\n") != std::string::npos &&
        editor->composition().empty(), "IME commit should insert UTF-8 once");
    tree.keyPressed(lotui::KeyCode::Backspace);
    require(editor->text().find("한") == std::string::npos,
        "Backspace should remove one complete UTF-8 code point");
    require(editor->undo() && editor->text().find("한") != std::string::npos,
        "Undo should restore the UTF-8 character");
    require(editor->redo() && editor->text().find("한") == std::string::npos,
        "Redo should restore the deletion");
    require(changes == 5, "each edit and history action should notify once");

    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(std::any_of(commands.begin(), commands.end(),
        [editor](const auto& command) {
            return command.color.blue == editor->style().keyword.blue &&
                command.color.red == editor->style().keyword.red &&
                command.texture == 41;
        }), "C++ keywords should use the code highlight color");
    require(tree.textInputState().inputRect.y > editor->bounds().y,
        "IME candidate should follow the editor caret");
}

void selectionClipboardAndFind() {
    auto engine = std::make_shared<TestEngine>();
    auto widget = std::make_unique<lotui::CodeEditor>(engine,
        "one\ntwo\none", "cpp");
    auto* editor = widget.get();
    std::string clipboard;
    editor->setClipboardHandlers(
        [&clipboard](std::string text) {
            clipboard = std::move(text);
            return true;
        },
        [&clipboard]() -> std::optional<std::string> { return clipboard; });
    lotui::WidgetTree tree(std::move(widget));
    tree.layout({0.0F, 0.0F, 420.0F, 160.0F});
    tree.keyPressed(lotui::KeyCode::Tab);
    require(editor->findNext("one") && editor->selectedText() == "one",
        "find should select the first match");
    require(editor->findNext("one") && editor->cursorByteOffset() == 11,
        "find should advance to the next match");
    require(editor->findNext("one") && editor->cursorByteOffset() == 3,
        "find should wrap at document end");
    tree.keyPressed(lotui::KeyCode::C, {false, true});
    require(clipboard == "one", "Ctrl+C should copy the selection");
    tree.keyPressed(lotui::KeyCode::X, {false, true});
    require(editor->text() == "\ntwo\none", "Ctrl+X should remove selected text");
    tree.keyPressed(lotui::KeyCode::V, {false, true});
    require(editor->text() == "one\ntwo\none",
        "Ctrl+V should insert clipboard text");
    tree.keyPressed(lotui::KeyCode::A, {false, true});
    require(editor->selectedText() == editor->text(),
        "Ctrl+A should select the complete document");
    tree.keyPressed(lotui::KeyCode::Tab);
    require(editor->text().find("    one\n    two\n    one") == 0,
        "Tab should indent all selected lines");
    tree.keyPressed(lotui::KeyCode::Tab, {true});
    require(editor->text() == "one\ntwo\none",
        "Shift+Tab should outdent selected lines");
    require(editor->undo() && editor->text().find("    one") == 0,
        "multi-line outdent should be one undo step");
}

void bracketsAndAutomaticIndentation() {
    auto engine = std::make_shared<TestEngine>();
    auto widget = std::make_unique<lotui::CodeEditor>(engine, "");
    auto* editor = widget.get();
    lotui::WidgetTree tree(std::move(widget));
    tree.layout({0.0F, 0.0F, 420.0F, 180.0F});
    tree.keyPressed(lotui::KeyCode::Tab);

    tree.textInput({lotui::TextInputEventType::Commit, "{"});
    require(editor->text() == "{}" && editor->cursorByteOffset() == 1,
        "opening brace should insert a pair and leave caret inside");
    tree.keyPressed(lotui::KeyCode::Enter);
    require(editor->text() == "{\n    \n}" && editor->cursorByteOffset() == 6,
        "Enter between braces should indent the body and align the close");
    tree.textInput({lotui::TextInputEventType::Commit, "("});
    tree.textInput({lotui::TextInputEventType::Commit, "["});
    require(editor->text() == "{\n    ([])\n}",
        "parentheses and square brackets should nest");
    tree.keyPressed(lotui::KeyCode::Backspace);
    require(editor->text() == "{\n    ()\n}",
        "Backspace inside an empty pair should remove both brackets");
    tree.textInput({lotui::TextInputEventType::Commit, ")"});
    require(editor->text() == "{\n    ()\n}" && editor->cursorByteOffset() == 8,
        "typing an existing close should move past it");
    require(editor->undo() && editor->text() == "{\n    ([])\n}",
        "skipping a close should not create an undo step");

    editor->setText("");
    tree.textInput({lotui::TextInputEventType::Commit, "{"});
    tree.keyPressed(lotui::KeyCode::Enter);
    tree.textInput({lotui::TextInputEventType::Commit, "}"});
    require(editor->text() == "{\n}" && editor->cursorByteOffset() == 3,
        "typing a close on the blank body should reuse the existing brace");

    editor->setText("if (ready) {");
    tree.keyPressed(lotui::KeyCode::End);
    tree.keyPressed(lotui::KeyCode::Enter);
    require(editor->text() == "if (ready) {\n    ",
        "Enter after an opening brace should add one indent level");
    tree.textInput({lotui::TextInputEventType::Commit, "}"});
    require(editor->text() == "if (ready) {\n}",
        "typing a close on an empty indented line should outdent");

    editor->setText("item");
    editor->selectAll();
    tree.textInput({lotui::TextInputEventType::Commit, "("});
    require(editor->text() == "(item)" && editor->selectedText() == "item",
        "opening bracket should wrap a selection and preserve it");
    tree.textInput({lotui::TextInputEventType::Commit, "value"});
    require(editor->text() == "(value)",
        "replacement of wrapped selection should retain brackets");
}

void scrollsAndClips() {
    auto engine = std::make_shared<TestEngine>();
    std::string content;
    for (int i = 0; i < 60; ++i)
        content += "int line = " + std::to_string(i) + ";\n";
    auto widget = std::make_unique<lotui::CodeEditor>(engine, content);
    auto* editor = widget.get();
    lotui::WidgetTree tree(std::move(widget));
    tree.layout({5.0F, 7.0F, 300.0F, 120.0F});
    editor->scrollToLine(40);
    require(editor->scrollOffset() > 0.0F,
        "scrollToLine should expose lower lines");
    const float before = editor->scrollOffset();
    tree.scroll({100.0F, 50.0F}, {0.0F, -2.0F},
        lotui::ScrollDeltaMode::Line);
    require(editor->scrollOffset() < before,
        "wheel scrolling should update the visible range");
    std::vector<lotui::PaintCommand> commands;
    tree.paint(commands);
    require(commands.size() < 100,
        "painting should emit only visible lines");
}

} // namespace

int main() {
    editsUtf8AndHistory();
    selectionClipboardAndFind();
    bracketsAndAutomaticIndentation();
    scrollsAndClips();
    return 0;
}
