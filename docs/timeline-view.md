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
```

The header is `widgets/timeline_view.h`. `setTracks()` validates IDs and finite,
nonnegative key times before replacing the current model. The selected key is
identified by track ID plus key ID. The ruler supports scrubbing, the wheel
zooms around the pointer, keys can be dragged, and the transport control or
Space toggles playback. Left/Right seek and Home returns to zero. Key dragging
updates the display during the gesture and calls `setOnKeyMoved()` once on
release. A cancelled drag restores the original time. The host owns playback,
Undo and model persistence.

Run `timeline_view_demo`, `timeline_view_demo --smoke`, or
`timeline_view_demo --capture output.ppm`. The same demo builds separately in
`LotGUI_Sample` against the static DesktopSDK through `find_package(LotUI)`.

The 3dEngine adapter lives in its `samples/lotui_smoke` directory. It reads
real timeline tracks through the engine C API and checks key drag, Undo/Redo,
scrubbing and zoom with `LotCADLotGUI --timeline-test`. More advanced editor
features such as interpolation controls, multi-key selection and clip lanes
remain outside this first control.
