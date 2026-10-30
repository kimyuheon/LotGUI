# ConversationList

`ConversationList` is a retained, scrollable message view for chat-style
interfaces. Messages have unique, nonempty stable IDs, sender names, timestamps, an incoming or
outgoing side, and ordered text and code blocks. Click a bubble to select it.
`appendMessage` keeps the view at the bottom when the reader was already there;
it preserves scroll position when reading older messages.
`ConversationListStyle` exposes body, metadata, and code font sizes so an
application can tune readability for its display density.
Set `incomingFullWidth` to give answer messages the full content width while
questions remain compact on the right. Text blocks wrap at word boundaries,
and long unbroken words wrap at UTF-8 code-point boundaries when resized.

```cpp
auto messages = std::vector<lotui::ConversationMessage>{
    {"m1", "Minji", "10:24", false,
        lotui::parseMessageBlocks(
            "A C++ example:\n```cpp\nint answer = 42;\n```")},
};
auto view = std::make_unique<lotui::ConversationList>(textEngine, messages);
view->setOnSelectionChanged([](std::string_view id) {
    // Open a message menu or update the application's selection.
});
view->setClipboardWriteHandler([nativeWindow](std::string code) {
    return nativeWindow->writeClipboardText(code);
});
```

Fenced blocks are recognized at the start of a line. Supported highlighting
includes C, C++, C#, Python, JavaScript, TypeScript, JSON, Bash, and SQL.
Unknown languages still render as labeled, plain code blocks. The parser only
recognizes fences; it does not render other Markdown syntax. Plugins can
construct `ConversationMessage` and `MessageBlock` directly without parsing.
Spaces and line breaks inside code are preserved. Tabs advance to four-column
stops, including mixed spaces and tabs. Code spaces use a fixed visual advance
so indentation remains clear even with the bundled proportional Korean font;
the stored message text is unchanged.
When a clipboard writer is set, each code block shows a copy-icon **복사** button.
It copies the original UTF-8 code, including indentation, and changes to a
green **복사됨** state after a successful write. The widget uses a callback so
plugins can provide their host application's clipboard without depending on an
OS API.

`conversation_demo` shows incoming and outgoing messages, Korean text, code
blocks, scrolling, selection, and a text composer. Enter or the upward-arrow
button appends the composed message. The demo can save a screenshot with:

```text
conversation_demo --capture output.ppm
```

`conversation_qa_demo` is a separate executable showing right-side questions
and full-width answers. It accepts the same `--smoke` and `--capture` flags.
