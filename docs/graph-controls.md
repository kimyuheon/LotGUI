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

The first control is now `PlotView`. It gives users a useful chart and proves
the line drawing path before the more complex node editor is migrated. See
`plot-view.md` for the current public API and remaining limitations.

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

## GraphCanvas follow-up

Build nodes, ports and links on the same stroke/path layer. Stable IDs,
hit-testing, pointer capture, box selection, pan/zoom, link drag and keyboard
navigation belong to LotUI. 3dEngine's `graph/lot_node_graph_view.*` remains
the source of geometry and graph semantics until a tested adapter proves the
replacement. An engine-only callback maps LotUI interactions to CAD commands
and Undo transactions. Plugins use the LotUI SDK model rather than linking
against 3dEngine classes.

Do not call the existing 3dEngine node editor an ImGui replacement milestone:
its current drawing is already outside ImGui. The migration is complete only
when its input and visual behavior work in the independent LotUI sample and the
embedded engine target.
