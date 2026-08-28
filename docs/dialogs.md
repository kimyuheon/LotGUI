# Dialogs

`DialogHost` owns the application content and at most one active modal widget.
While a modal is visible, the host paints a scrim above the application,
routes pointer input only to the modal, and exposes only modal controls to
keyboard focus traversal. Closing the modal restores the control that was
focused before it opened.

The modal content remains ordinary retained-mode widgets:

```cpp
auto applicationContent = std::make_unique<lotui::Column>();
auto host = std::make_unique<lotui::DialogHost>(
    std::move(applicationContent));
lotui::DialogHost* dialogHost = host.get();

auto actions = std::make_unique<lotui::Row>();
actions->addChild(std::make_unique<lotui::Button>(
    lotui::Size{96.0F, 40.0F},
    [dialogHost]() { dialogHost->acceptModal(); }));
actions->addChild(std::make_unique<lotui::Button>(
    lotui::Size{96.0F, 40.0F},
    [dialogHost]() { dialogHost->cancelModal(); }));

dialogHost->showModal(
    std::make_unique<lotui::Dialog>(
        std::move(actions), lotui::Size{420.0F, 240.0F}),
    [](lotui::DialogResult result) {
        if (result == lotui::DialogResult::Accepted) {
            saveProject();
        }
    });

lotui::WidgetTree tree(std::move(host));
```

`acceptModal()`, `cancelModal()`, and `dismissModal()` are safe inside a button
callback and report `Accepted`, `Cancelled`, or `Dismissed` exactly once.
Escape reports `Cancelled` by default; set `DialogHostStyle::cancelOnEscape` to
`false` for a dialog that must receive an explicit decision. `takeModal()` is
available when the caller needs to recover ownership without reporting a
result.

This foundation intentionally does not yet provide a title bar, result future,
default-button policy, nested modals, or modeless floating dialogs. Those
features belong to the higher-level dialog controller built on this host.
