# Graph controls for LotUI

The 3dEngine node graph in `../3dEngine/first_app/node_graph_ui.cpp` is already
drawn through the engine's Vulkan overlay, independently of ImGui. LotUI should
not import that engine model. The engine also draws small legends and previews
with ImGui draw lists. These needs suggest two distinct LotUI consumers:

1. **PlotView**: time series, simulation results, profiles, and other numeric
   charts. Its public model owns series data, axes, ranges, and selection.
2. **GraphCanvas**: a reusable surface for nodes, ports, and curved connections.
   The 3dEngine adapter translates its existing graph view model into LotUI
   items. Engine graph algorithms and Undo state remain in 3dEngine.

`PlotView` and the first `GraphCanvas` control are available. See `plot-view.md`
and `graph-canvas.md` for their public APIs and remaining limitations.

## Drawing foundation

`PaintCommand` now supports straight line segments with width, color and clip
alongside the existing rectangles. Both the standalone Vulkan renderer and the
embedded renderer consume the same command; widgets contain no Vulkan types.
The remaining drawing foundation is a batched polyline/path command with
shared joins. This should preserve the existing logical-coordinate-to-physical-
pixel transform and rectangle/text behavior.

The current line renderer rotates an instanced rectangle segment. A later path
renderer can add round joins, dashed lines, Bezier links, and filled areas
without changing the plot data model. Adjacent segments should share joins and
be batched by clip/texture/material rather than submitted per point.

## PlotView first milestone

- Public C++ API for multiple named series of `(x, y)` points, with stable
  series IDs and colors. Data and labels are UTF-8; no CAD types appear.
- Numeric X/Y axes, grid and tick labels, auto range and explicit range.
  Empty, constant, invalid and sparse data must have defined behavior.
- Pointer hover with nearest sample and crosshair; wheel zoom and pointer pan
  consume input only inside the plot. Expose selection/viewport callbacks for
  plugins and hosts.
- Clip every series to the plot rectangle. Reduce dense series to a bounded
  number of visible extrema per screen pixel so spikes remain visible.
- Show the same plot in a standalone example and `LotGUI_Sample` using only the
  public SDK. Validate keyboard focus and DPI on supported platforms.
- Add a separate `LotCADLotGUI` example that reads actual object dimensions
  through `CAD_GetBoundsWorld`, refreshes on scene revisions, and leaves the
  existing CAD executable and native graph logic intact.

The embedded sample now has a project object checklist and a `--plot` mode for
existing projects. The standalone SDK example has a matching object checklist
with local example data.

## GraphCanvas integration follow-up

The first canvas draws nodes, ports and links with stable IDs, hit testing,
pointer capture, selection, node dragging, pan/zoom and keyboard navigation.
Box selection and link dragging remain. 3dEngine's `graph/lot_node_graph_view.*` remains
the source of geometry and graph semantics until a tested adapter proves the
replacement. An engine-only callback maps LotUI interactions to CAD commands
and Undo transactions. Plugins use the LotUI SDK model rather than linking
against 3dEngine classes.

`LotCADLotGUI --graph` visualizes recent scene objects via the CAD scene-tree
API. The separate `--node-graph` mode now reads the engine's actual node model
and kind metadata through the public CAD graph API. `--node-graph-test` creates
a four-node geometry flow, checks rendering, a drag and one-step Undo/Redo,
then captures the embedded view. Port linking, parameter editing and the
engine editor's other interactions are still pending.

Do not call the existing 3dEngine node editor an ImGui replacement milestone:
its current drawing is already outside ImGui. The migration is complete only
when its input and visual behavior work in the independent LotUI sample and the
embedded engine target.
