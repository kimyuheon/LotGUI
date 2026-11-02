# PlotView

`PlotView` is a retained, renderer-neutral numeric graph. It is available from
`widgets/plot_view.h` or the `lotui/lotui.h` umbrella header. Plugins can use
the same C++ SDK as standalone applications; the widget has no CAD types.

```cpp
auto plot = std::make_unique<lotui::PlotView>(textEngine);
plot->setSeries({
    {"temperature", "Temperature", {{0.0, 23.0}, {1.0, 28.0}},
        {0.98F, 0.67F, 0.32F, 1.0F}, true}
});
plot->setOnViewportChanged([](lotui::PlotRange range) {
    // Save or display the user's visible range.
});
```

X values in each series must be finite and sorted. Nonfinite Y values break a
line into separate spans. Auto range ignores hidden series and invalid Y
values; `setRange()` selects a manual range and `resetAutoRange()` restores the
data range. A dense series keeps its first/last points and vertical extrema in
each screen column, so a narrow spike remains visible.

The graph has four X and Y grid intervals and numeric labels. The pointer can
hover the nearest sample, drag to pan, and wheel to zoom around the cursor.
Tab focuses the graph, arrows pan, PageUp/PageDown zoom, and Home restores the
full data range. `setOnHoverChanged()` exposes the nearest sample to host UI.

`linePaintCommand()` supplies a renderer-neutral line primitive. Both Vulkan
renderers consume it through the same paint batch planner and clip rectangles.
Run `plot_view_demo --smoke` or `plot_view_demo --capture output.ppm` to verify
the standalone path. In the engine checkout, `LotCADLotGUI --plot-test
output.png` exercises the embedded path with actual CAD object bounds, checks
refresh after an object is added, and verifies wheel zoom and drag pan.

The current plot uses straight segments, numeric axes, and CPU-side
per-column reduction. Time/date axes, legends inside the widget, line joins,
Bezier links, and large streaming buffers are future work.
