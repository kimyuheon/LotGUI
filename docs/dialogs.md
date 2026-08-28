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

auto dialog = std::make_unique<lotui::Dialog>(
    std::move(actions), lotui::Size{420.0F, 240.0F});
dialog->setTitle(std::make_unique<lotui::Label>(
    textEngine, "프로젝트 저장"));

dialogHost->showModal(
    std::move(dialog),
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

`Dialog::setTitle()` accepts any widget, so applications can use a `Label` or a
full `Row` containing a title, status, and close button. The title has separate
padding and a divider and does not impose a text engine on the core dialog.

Set `DialogHostStyle::acceptOnUnhandledEnter` to `true` when Enter should act
as the default confirmation. Focused controls get the key first: a focused
button still activates itself and a text field can submit without being
overridden. Only an otherwise unhandled Enter reports `Accepted`.

This foundation intentionally does not yet provide a result future, nested
modals, or asynchronous waiting.

## Modeless dialogs

`showModeless()` returns a stable ID used to update, raise, or close a floating
dialog. Multiple modeless dialogs remain interactive with the application and
paint in Z-order. Pressing any control in a modeless dialog automatically
brings that dialog to the front.

```cpp
const auto propertiesId = dialogHost->showModeless(
    std::move(propertiesDialog),
    {880.0F, 72.0F, 360.0F, 640.0F},
    [] { savePanelPlacement(); });

dialogHost->setModelessBounds(
    propertiesId, {920.0F, 80.0F, 380.0F, 680.0F});
dialogHost->bringModelessToFront(propertiesId);
dialogHost->closeModeless(propertiesId);
```

An active modal is always painted above modeless dialogs and temporarily
blocks their pointer and keyboard focus. Title-bar dragging and resize handles
are the next layer on top of the modeless bounds API.
