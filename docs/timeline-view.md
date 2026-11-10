# TimelineView

`TimelineView` is a retained C++ keyframe timeline. Its public `TimelineTrack`
and `TimelineKey` types contain stable string IDs and seconds. The widget has
no CAD, Vulkan or operating system type in its API, so a plugin or host can
translate its own animation model into tracks.

```cpp
auto view = std::make_unique<lotui::TimelineView>(textEngine);
view->setDuration(12.0);
view->setTracks({{"camera", "Camera",
    {{"start", 0.0}, {"turn", 4.0}, {"end", 12.0}}}});
view->setOnTimeChanged([](double seconds) { /* seek the host */ });
view->setOnPlayChanged([](bool playing) { /* control playback */ });
view->setOnKeyMoved([](const lotui::TimelineKeyRef& key, double seconds) {
    // Commit one host Undo operation when the drag ends.
});
view->setOnKeysMoved([](const std::vector<lotui::TimelineKeyRef>& keys,
        double deltaSeconds) {
    // Commit a group drag as one host Undo operation.
});
view->setOnInterpolationChanged([](const auto& keys,
        lotui::TimelineInterpolation mode) { /* update the host */ });
view->setOnKeysDeleted([](const auto& keys) { /* delete in the host */ });
```

The header is `widgets/timeline_view.h`. `setTracks()` validates IDs and finite,
nonnegative key times before replacing the current model. The selected key is
identified by track ID plus key ID. Ctrl/Command click adds or removes keys
from the selection; dragging any selected key moves the group without changing
their spacing. The ruler supports scrubbing, the wheel zooms around the
pointer, and the three compact header controls toggle playback, cycle the
selected keys through smooth/linear/step interpolation, and delete them.
Space toggles playback, F2 cycles interpolation, Delete removes selected keys,
Left/Right seek and Home returns to zero. Key dragging previews locally and
calls `setOnKeysMoved()` once on release when that callback is installed. A
cancelled drag restores the original times. The host owns playback, Undo and
model persistence.

Run `timeline_view_demo`, `timeline_view_demo --smoke`, or
`timeline_view_demo --capture output.ppm`, or
`timeline_view_demo --capture-edits output.ppm` to capture a multi-key
selection. The same demo builds separately in
`LotGUI_Sample` against the static DesktopSDK through `find_package(LotUI)`.

The 3dEngine adapter lives in its `samples/lotui_smoke` directory. It reads
real timeline tracks through the engine C API and checks group drag,
interpolation, deletion, Undo/Redo, scrubbing and zoom with
`LotCADLotGUI --timeline-test`. Clip lanes and range selection remain outside
this control.
