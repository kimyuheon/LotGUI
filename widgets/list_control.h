#pragma once

#include "core/widget.h"
#include "text/text_layout.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lotui {

enum class ListCellKind {
    Text,
    CheckBox,
    ComboBox,
    Lookup,
    ActionButton,
};

struct ListColumn {
    std::string id;
    std::string title;
    float width{120.0F};
    float minimumWidth{40.0F};
    bool resizable{true};
    bool sortable{true};
};

enum class ListSortDirection {
    Ascending,
    Descending,
};

struct ListSortDescriptor {
    std::size_t column{0};
    ListSortDirection direction{ListSortDirection::Ascending};
};

constexpr bool operator==(
    ListSortDescriptor left,
    ListSortDescriptor right) noexcept {
    return left.column == right.column && left.direction == right.direction;
}

constexpr bool operator!=(
    ListSortDescriptor left,
    ListSortDescriptor right) noexcept {
    return !(left == right);
}

struct ListCell {
    ListCellKind kind{ListCellKind::Text};
    std::string text;
    bool checked{false};
    std::vector<std::string> options;
    std::size_t selectedOption{0};
    bool editable{true};
};

using ListRow = std::vector<ListCell>;

struct ListCellAddress {
    std::size_t row{0};
    std::size_t column{0};
};

constexpr bool operator==(
    ListCellAddress left,
    ListCellAddress right) noexcept {
    return left.row == right.row && left.column == right.column;
}

constexpr bool operator!=(
    ListCellAddress left,
    ListCellAddress right) noexcept {
    return !(left == right);
}

enum class ListCellAction {
    Activate,
    BeginEdit,
    ToggleCheck,
    OpenComboBox,
    OpenLookup,
    InvokeButton,
};

struct ListCellEvent {
    ListCellAddress address{};
    ListCellAction action{ListCellAction::Activate};
    Rect anchor{};
};

struct ListControlStyle {
    Color background{0.09F, 0.11F, 0.16F, 1.0F};
    Color headerBackground{0.14F, 0.17F, 0.23F, 1.0F};
    Color alternateRow{0.11F, 0.13F, 0.18F, 1.0F};
    Color hoveredCell{0.18F, 0.24F, 0.34F, 1.0F};
    Color selectedCell{0.15F, 0.38F, 0.70F, 1.0F};
    Color selectedCellFocused{0.13F, 0.46F, 0.90F, 1.0F};
    Color gridLine{0.24F, 0.27F, 0.34F, 1.0F};
    Color text{0.91F, 0.93F, 0.97F, 1.0F};
    Color secondaryText{0.66F, 0.70F, 0.78F, 1.0F};
    Color controlBackground{0.18F, 0.22F, 0.29F, 1.0F};
    Color controlAccent{0.32F, 0.65F, 1.0F, 1.0F};
    Color focusRing{0.96F, 0.82F, 0.32F, 1.0F};
    Color scrollBarTrack{0.12F, 0.14F, 0.19F, 1.0F};
    Color scrollBarThumb{0.34F, 0.39F, 0.48F, 1.0F};
    Color scrollBarThumbHovered{0.43F, 0.50F, 0.62F, 1.0F};
    Color scrollBarThumbPressed{0.32F, 0.65F, 1.0F, 1.0F};
    Color headerHovered{0.18F, 0.22F, 0.30F, 1.0F};
    Color headerPressed{0.21F, 0.29F, 0.40F, 1.0F};
    Color sortIndicator{0.52F, 0.72F, 1.0F, 1.0F};
    Color columnResizeHandle{0.32F, 0.65F, 1.0F, 1.0F};
    EdgeInsets cellPadding{8.0F, 4.0F, 8.0F, 4.0F};
    float headerHeight{30.0F};
    float rowHeight{28.0F};
    float controlSize{18.0F};
    float gridLineWidth{1.0F};
    float focusRingWidth{2.0F};
    float cornerRadius{6.0F};
    float scrollBarThickness{10.0F};
    float minimumScrollThumbLength{22.0F};
    float columnResizeHandleWidth{2.0F};
    float columnResizeHitWidth{8.0F};
};

class ListControl final : public Widget {
public:
    using SelectionChangedHandler =
        std::function<void(std::optional<ListCellAddress>)>;
    using CellActionHandler = std::function<void(const ListCellEvent&)>;
    using SortChangedHandler =
        std::function<void(std::optional<ListSortDescriptor>)>;
    using ColumnResizedHandler =
        std::function<void(std::size_t column, float width)>;

    explicit ListControl(
        std::shared_ptr<const TextEngine> textEngine,
        std::vector<ListColumn> columns = {},
        std::vector<ListRow> rows = {},
        Size preferredSize = {480.0F, 280.0F},
        SelectionChangedHandler onSelectionChanged = {},
        CellActionHandler onCellAction = {},
        ListControlStyle style = {},
        TextStyle textStyle = {});

    Size measure(const LayoutConstraints& constraints) const override;

    void setColumns(std::vector<ListColumn> columns);
    const std::vector<ListColumn>& columns() const noexcept;
    void setRows(std::vector<ListRow> rows);
    const std::vector<ListRow>& rows() const noexcept;
    void setColumnWidth(std::size_t column, float width);
    float columnWidth(std::size_t column) const noexcept;
    Rect headerCellBounds(std::size_t column) const noexcept;
    Rect columnResizeHandleBounds(std::size_t column) const noexcept;
    void setCell(ListCellAddress address, ListCell cell);
    const ListCell* cell(ListCellAddress address) const noexcept;

    void setSelectedCell(std::optional<ListCellAddress> address);
    std::optional<ListCellAddress> selectedCell() const noexcept;
    void setComboSelection(ListCellAddress address, std::size_t option);
    void setCellText(ListCellAddress address, std::string text);
    void setChecked(ListCellAddress address, bool checked);

    void setScrollOffset(Point offset) noexcept;
    Point scrollOffset() const noexcept;
    Size contentSize() const noexcept;
    void ensureCellVisible(ListCellAddress address) noexcept;

    bool hasVerticalScrollBar() const noexcept;
    bool hasHorizontalScrollBar() const noexcept;
    Rect verticalScrollBarBounds() const noexcept;
    Rect verticalScrollThumbBounds() const noexcept;
    Rect horizontalScrollBarBounds() const noexcept;
    Rect horizontalScrollThumbBounds() const noexcept;

    Rect cellBounds(ListCellAddress address) const noexcept;
    Rect cellActionBounds(ListCellAddress address) const noexcept;

    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;
    bool isFocused() const noexcept;
    void setPreferredSize(Size preferredSize) noexcept;
    void setOnSelectionChanged(SelectionChangedHandler handler);
    void setOnCellAction(CellActionHandler handler);
    void setSortDescriptor(std::optional<ListSortDescriptor> descriptor);
    std::optional<ListSortDescriptor> sortDescriptor() const noexcept;
    void setOnSortChanged(SortChangedHandler handler);
    void setOnColumnResized(ColumnResizedHandler handler);
    void setStyle(ListControlStyle style) noexcept;
    const ListControlStyle& style() const noexcept;

protected:
    void onArrange() override;
    void onPaint(std::vector<PaintCommand>& commands) const override;
    bool acceptsPointerEvents() const noexcept override;
    bool onPointerEvent(const WidgetPointerEvent& event) override;
    bool acceptsFocus() const noexcept override;
    bool onFocusChanged(bool focused) override;
    bool onKeyEvent(const WidgetKeyEvent& event) override;
    bool onScrollEvent(const WidgetScrollEvent& event) override;

private:
    enum class ScrollBarPart {
        None,
        VerticalTrack,
        VerticalThumb,
        HorizontalTrack,
        HorizontalThumb,
        Corner,
    };

    enum class ScrollDragAxis {
        None,
        Vertical,
        Horizontal,
    };

    struct ScrollGeometry {
        Rect viewport{};
        Rect header{};
        Rect data{};
        Rect verticalTrack{};
        Rect verticalThumb{};
        Rect horizontalTrack{};
        Rect horizontalThumb{};
        Rect corner{};
        bool vertical{false};
        bool horizontal{false};
    };

    struct LayoutCache {
        std::string text;
        float maximumWidth{-1.0F};
        std::unique_ptr<TextLayout> layout;
    };

    Rect contentBounds() const noexcept;
    ScrollGeometry scrollGeometry() const noexcept;
    ScrollBarPart scrollBarPartAt(Point position) const noexcept;
    bool updateScrollBarHover(Point position) noexcept;
    bool dragScrollBar(Point position) noexcept;
    bool pageScrollBar(ScrollBarPart part, Point position) noexcept;
    bool scrollBy(Point delta) noexcept;
    void paintScrollBars(
        const ScrollGeometry& geometry,
        Rect paintClip,
        std::vector<PaintCommand>& commands) const;
    std::optional<ListCellAddress> addressAt(Point position) const noexcept;
    std::optional<std::size_t> headerColumnAt(Point position) const noexcept;
    std::optional<std::size_t> resizeColumnAt(Point position) const noexcept;
    bool updateHeaderHover(Point position) noexcept;
    bool dragColumnResize(Point position);
    bool toggleSort(std::size_t column);
    void paintSortIndicator(
        std::size_t column,
        Rect headerCell,
        Rect paintClip,
        std::vector<PaintCommand>& commands) const;
    bool isValidAddress(ListCellAddress address) const noexcept;
    float columnStart(std::size_t column) const noexcept;
    void clampScrollOffset() noexcept;
    bool select(std::optional<ListCellAddress> address, bool notify);
    bool moveSelection(int rowDelta, int columnDelta);
    bool activateCell(ListCellAddress address, bool editText);
    ListCellAction defaultAction(
        const ListCell& cell,
        bool editText) const noexcept;
    void notifyCellAction(ListCellAddress address, ListCellAction action);
    void invalidateLayouts() const noexcept;
    const TextLayout& ensureLayout(
        std::size_t cacheIndex,
        std::string_view text,
        float maximumWidth) const;
    std::size_t cacheIndex(
        std::size_t row,
        std::size_t column) const noexcept;
    std::string displayText(const ListCell& cell) const;
    void paintText(
        std::size_t cacheIndex,
        std::string_view text,
        Rect bounds,
        Rect paintClip,
        Color color,
        std::vector<PaintCommand>& commands) const;
    void paintCellControl(
        const ListCell& cell,
        Rect bounds,
        Rect paintClip,
        std::vector<PaintCommand>& commands) const;

    std::shared_ptr<const TextEngine> textEngine_;
    std::vector<ListColumn> columns_;
    std::vector<ListRow> rows_;
    Size preferredSize_{480.0F, 280.0F};
    SelectionChangedHandler onSelectionChanged_{};
    CellActionHandler onCellAction_{};
    SortChangedHandler onSortChanged_{};
    ColumnResizedHandler onColumnResized_{};
    ListControlStyle style_{};
    TextStyle textStyle_{};
    std::optional<ListCellAddress> selectedCell_;
    std::optional<ListCellAddress> hoveredCell_;
    std::optional<ListCellAddress> pressedCell_;
    Point scrollOffset_{};
    bool enabled_{true};
    bool focused_{false};
    bool pointerPressed_{false};
    ScrollBarPart hoveredScrollBarPart_{ScrollBarPart::None};
    ScrollDragAxis scrollDragAxis_{ScrollDragAxis::None};
    float scrollDragPointerStart_{0.0F};
    float scrollDragOffsetStart_{0.0F};
    std::optional<std::size_t> hoveredHeaderColumn_;
    std::optional<std::size_t> hoveredResizeColumn_;
    std::optional<std::size_t> pressedHeaderColumn_;
    std::optional<std::size_t> resizingColumn_;
    float columnResizePointerStart_{0.0F};
    float columnResizeWidthStart_{0.0F};
    std::optional<ListSortDescriptor> sortDescriptor_;
    std::optional<ListCellAddress> lastClickedCell_;
    std::uint64_t lastClickMilliseconds_{0};
    mutable std::vector<LayoutCache> layouts_;
};

} // namespace lotui
