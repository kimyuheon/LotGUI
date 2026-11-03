# GraphCanvas

`GraphCanvas` is a retained, renderer-neutral node graph widget. It uses public
`GraphNode` and `GraphLink` data with stable string IDs; applications and plugins
can translate their own graph models into these records. No CAD or Vulkan type
appears in `widgets/graph_canvas.h`.

```cpp
auto graph = std::make_unique<lotui::GraphCanvas>(textEngine);
graph->setGraph({
    {"source", "Source", {20, 40}, {176, 112}, {}, {"data"}},
    {"result", "Result", {300, 40}, {176, 112}, {"data"}, {}}
}, {{"flow", "source", 0, "result", 0}});
graph->setOnNodeMoved([](std::string_view id, lotui::Point position) {
    // Commit the new position to the host model or Undo transaction.
});
graph->setOnSelectionChanged([](std::optional<std::string> id) {
    // Synchronize host selection.
});
```

`setGraph()` validates node IDs, sizes, endpoints and port indices before it
replaces the current model. An existing selection survives a refresh when its
node ID remains. The model uses canvas coordinates. `nodeBounds()` returns
screen bounds after pan and zoom. `fitToContent()` places all nodes in view.

Click a node to select it, drag to move it, drag empty space to pan, and use the
wheel to zoom around the pointer. Home fits the graph; Escape clears selection;
arrow keys move the selected node or pan the canvas. The widget consumes only
input inside its own bounds, so surrounding viewport input remains available.

Run `graph_canvas_demo`, `graph_canvas_demo --smoke`, or
`graph_canvas_demo --capture output.ppm`. The same source builds in the public
`LotGUI_Sample` repository with `find_package(LotUI CONFIG REQUIRED)`.

This first version draws cubic links as short straight strokes. Port dragging,
link creation, box selection, and host Undo grouping remain follow-up work. The
engine's existing node graph model must be adapted in the engine repository;
this SDK control does not import it.
