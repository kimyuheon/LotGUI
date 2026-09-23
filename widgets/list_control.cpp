#include "widgets/list_control.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lotui {
namespace {

float dimension(float value, float fallback = 0.0F) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value) : fallback;
}

void sanitize(ListControlStyle& style) noexcept {
    style.cellPadding.left = dimension(style.cellPadding.left);
    style.cellPadding.top = dimension(style.cellPadding.top);
    style.cellPadding.right = dimension(style.cellPadding.right);
    style.cellPadding.bottom = dimension(style.cellPadding.bottom);
    style.headerHeight = std::max(1.0F, dimension(style.headerHeight, 30.0F));
    style.rowHeight = std::max(1.0F, dimension(style.rowHeight, 28.0F));
    style.controlSize = std::max(1.0F, dimension(style.controlSize, 18.0F));
    style.gridLineWidth = dimension(style.gridLineWidth);
    style.focusRingWidth = dimension(style.focusRingWidth);
    style.cornerRadius = dimension(style.cornerRadius);
    style.scrollBarThickness = std::max(
        1.0F, dimension(style.scrollBarThickness, 10.0F));
    style.minimumScrollThumbLength = std::max(
        1.0F, dimension(style.minimumScrollThumbLength, 22.0F));
    style.columnResizeHandleWidth = dimension(
        style.columnResizeHandleWidth, 2.0F);
    style.columnResizeHitWidth = std::max(
        1.0F,
        dimension(style.columnResizeHitWidth, 8.0F));
    style.columnResizeHitWidth = std::max(
        style.columnResizeHitWidth, style.columnResizeHandleWidth);
    style.columnReorderDragThreshold = dimension(
        style.columnReorderDragThreshold, 6.0F);
    style.columnReorderIndicatorWidth = dimension(
        style.columnReorderIndicatorWidth, 3.0F);
    style.frozenColumnDividerWidth = dimension(
        style.frozenColumnDividerWidth, 2.0F);
}

void sanitize(ListColumn& column) noexcept {
    column.minimumWidth = std::max(
        1.0F, dimension(column.minimumWidth, 40.0F));
    column.width = std::max(
        column.minimumWidth, dimension(column.width, column.minimumWidth));
}

Rect inset(Rect bounds, float amount) noexcept {
    const float insetAmount = std::min(
        amount, std::min(bounds.width, bounds.height) * 0.5F);
    return {
        bounds.x + insetAmount,
        bounds.y + insetAmount,
        std::max(0.0F, bounds.width - insetAmount * 2.0F),
        std::max(0.0F, bounds.height - insetAmount * 2.0F),
    };
}

std::uint64_t monotonicMilliseconds() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

constexpr std::uint64_t doubleClickIntervalMilliseconds = 500;

} // namespace

ListControl::ListControl(
    std::shared_ptr<const TextEngine> textEngine,
    std::vector<ListColumn> columns,
    std::vector<ListRow> rows,
    Size preferredSize,
    SelectionChangedHandler onSelectionChanged,
    CellActionHandler onCellAction,
    ListControlStyle style,
    TextStyle textStyle)
    : textEngine_(std::move(textEngine)),
      columns_(std::move(columns)),
      rows_(std::move(rows)),
      preferredSize_(preferredSize),
      onSelectionChanged_(std::move(onSelectionChanged)),
      onCellAction_(std::move(onCellAction)),
      style_(style),
      textStyle_(std::move(textStyle)) {
    if (!textEngine_) {
        throw std::invalid_argument("ListControl requires a TextEngine");
    }
    if (!isValidTextStyle(textStyle_)) {
        throw std::invalid_argument("ListControl text style is invalid");
    }
    sanitize(style_);
    for (ListColumn& column : columns_) {
        sanitize(column);
    }
    for (ListRow& row : rows_) {
        row.resize(columns_.size());
    }
}

Size ListControl::measure(const LayoutConstraints& constraints) const {
    return constraints.constrain({
        dimension(preferredSize_.width),
        dimension(preferredSize_.height),
    });
}

void ListControl::setColumns(std::vector<ListColumn> columns) {
    for (ListColumn& column : columns) {
        sanitize(column);
    }
    columns_ = std::move(columns);
    for (ListRow& row : rows_) {
        row.resize(columns_.size());
    }
    if (selectedCell_ && !isValidAddress(*selectedCell_)) {
        select(std::nullopt, true);
    }
    hoveredCell_.reset();
    pressedCell_.reset();
    pointerPressed_ = false;
    hoveredScrollBarPart_ = ScrollBarPart::None;
    scrollDragAxis_ = ScrollDragAxis::None;
    hoveredHeaderColumn_.reset();
    hoveredResizeColumn_.reset();
    pressedHeaderColumn_.reset();
    resizingColumn_.reset();
    reorderingColumn_.reset();
    reorderTargetColumn_.reset();
    frozenColumnCount_ = std::min(frozenColumnCount_, columns_.size());
    if (sortDescriptor_ &&
        (sortDescriptor_->column >= columns_.size() ||
         !columns_[sortDescriptor_->column].sortable)) {
        sortDescriptor_.reset();
    }
    lastClickedCell_.reset();
    lastClickMilliseconds_ = 0;
    invalidateLayouts();
    clampScrollOffset();
}

const std::vector<ListColumn>& ListControl::columns() const noexcept {
    return columns_;
}

void ListControl::setRows(std::vector<ListRow> rows) {
    for (ListRow& row : rows) {
        row.resize(columns_.size());
    }
    rows_ = std::move(rows);
    if (selectedCell_ && !isValidAddress(*selectedCell_)) {
        select(std::nullopt, true);
    }
    hoveredCell_.reset();
    pressedCell_.reset();
    pointerPressed_ = false;
    hoveredScrollBarPart_ = ScrollBarPart::None;
    scrollDragAxis_ = ScrollDragAxis::None;
    lastClickedCell_.reset();
    lastClickMilliseconds_ = 0;
    invalidateLayouts();
    clampScrollOffset();
}

const std::vector<ListRow>& ListControl::rows() const noexcept {
    return rows_;
}

void ListControl::setColumnWidth(std::size_t column, float width) {
    if (column >= columns_.size()) {
        throw std::out_of_range("ListControl column is out of range");
    }
    ListColumn& value = columns_[column];
    value.width = std::max(
        value.minimumWidth, dimension(width, value.minimumWidth));
    invalidateLayouts();
    clampScrollOffset();
}

float ListControl::columnWidth(std::size_t column) const noexcept {
    return column < columns_.size() ? columns_[column].width : 0.0F;
}

void ListControl::moveColumn(std::size_t from, std::size_t to) {
    if (from >= columns_.size() || to >= columns_.size()) {
        throw std::out_of_range("ListControl column is out of range");
    }
    if (from == to) {
        return;
    }
    const auto moveValue = [from, to](auto& values) {
        if (from < to) {
            std::rotate(
                values.begin() + static_cast<std::ptrdiff_t>(from),
                values.begin() + static_cast<std::ptrdiff_t>(from + 1),
                values.begin() + static_cast<std::ptrdiff_t>(to + 1));
        } else {
            std::rotate(
                values.begin() + static_cast<std::ptrdiff_t>(to),
                values.begin() + static_cast<std::ptrdiff_t>(from),
                values.begin() + static_cast<std::ptrdiff_t>(from + 1));
        }
    };
    moveValue(columns_);
    for (ListRow& row : rows_) {
        moveValue(row);
    }
    const auto remapAddress = [from, to](
        std::optional<ListCellAddress>& address) {
        if (address) {
            address->column = remapColumnIndex(address->column, from, to);
        }
    };
    remapAddress(selectedCell_);
    remapAddress(hoveredCell_);
    remapAddress(pressedCell_);
    remapAddress(lastClickedCell_);
    if (sortDescriptor_) {
        sortDescriptor_->column = remapColumnIndex(
            sortDescriptor_->column, from, to);
    }
    invalidateLayouts();
    clampScrollOffset();
}

void ListControl::setFrozenColumnCount(std::size_t count) noexcept {
    frozenColumnCount_ = std::min(count, columns_.size());
    clampScrollOffset();
}

std::size_t ListControl::frozenColumnCount() const noexcept {
    return frozenColumnCount_;
}

Rect ListControl::headerCellBounds(std::size_t column) const noexcept {
    if (column >= columns_.size()) {
        return {};
    }
    const ScrollGeometry geometry = scrollGeometry();
    return {
        columnViewportX(column, geometry),
        geometry.header.y,
        columnWidth(column),
        geometry.header.height,
    };
}

Rect ListControl::columnResizeHandleBounds(
    std::size_t column) const noexcept {
    if (column >= columns_.size() || !columns_[column].resizable) {
        return {};
    }
    const Rect headerCell = headerCellBounds(column);
    const float width = style_.columnResizeHitWidth;
    return {
        headerCell.x + headerCell.width - width * 0.5F,
        headerCell.y,
        width,
        headerCell.height,
    };
}

void ListControl::setCell(ListCellAddress address, ListCell cellValue) {
    if (!isValidAddress(address)) {
        throw std::out_of_range("ListControl cell address is out of range");
    }
    rows_[address.row][address.column] = std::move(cellValue);
    invalidateLayouts();
}

const ListCell* ListControl::cell(ListCellAddress address) const noexcept {
    return isValidAddress(address)
        ? &rows_[address.row][address.column]
        : nullptr;
}

void ListControl::setSelectedCell(
    std::optional<ListCellAddress> address) {
    if (address && !isValidAddress(*address)) {
        throw std::out_of_range("ListControl selection is out of range");
    }
    if (select(address, true) && address) {
        ensureCellVisible(*address);
    }
}

std::optional<ListCellAddress> ListControl::selectedCell() const noexcept {
    return selectedCell_;
}

void ListControl::setComboSelection(
    ListCellAddress address,
    std::size_t option) {
    if (!isValidAddress(address)) {
        throw std::out_of_range("ListControl cell address is out of range");
    }
    ListCell& value = rows_[address.row][address.column];
    if (value.kind != ListCellKind::ComboBox) {
        throw std::invalid_argument("ListControl cell is not a combo box");
    }
    if (option >= value.options.size()) {
        throw std::out_of_range("ListControl combo option is out of range");
    }
    value.selectedOption = option;
    invalidateLayouts();
}

void ListControl::setCellText(
    ListCellAddress address,
    std::string text) {
    if (!isValidAddress(address)) {
        throw std::out_of_range("ListControl cell address is out of range");
    }
    rows_[address.row][address.column].text = std::move(text);
    invalidateLayouts();
}

void ListControl::setChecked(ListCellAddress address, bool checked) {
    if (!isValidAddress(address)) {
        throw std::out_of_range("ListControl cell address is out of range");
    }
    ListCell& value = rows_[address.row][address.column];
    if (value.kind != ListCellKind::CheckBox) {
        throw std::invalid_argument("ListControl cell is not a check box");
    }
    value.checked = checked;
}

void ListControl::setScrollOffset(Point offset) noexcept {
    scrollOffset_ = {dimension(offset.x), dimension(offset.y)};
    clampScrollOffset();
}

Point ListControl::scrollOffset() const noexcept {
    return scrollOffset_;
}

Size ListControl::contentSize() const noexcept {
    float width = 0.0F;
    for (const ListColumn& column : columns_) {
        width += column.width;
    }
    return {width, static_cast<float>(rows_.size()) * style_.rowHeight};
}

void ListControl::ensureCellVisible(ListCellAddress address) noexcept {
    if (!isValidAddress(address)) {
        return;
    }
    const Rect data = scrollGeometry().data;
    const float left = columnStart(address.column);
    const float right = left + columnWidth(address.column);
    const float top = static_cast<float>(address.row) * style_.rowHeight;
    const float bottom = top + style_.rowHeight;

    const float frozenWidth = frozenColumnsWidth();
    if (address.column >= frozenColumnCount_ &&
        scrollGeometry().scrollableViewportWidth > 0.0F) {
        const float scrollableLeft = left - frozenWidth;
        const float scrollableRight = right - frozenWidth;
        const float visibleWidth = scrollGeometry().scrollableViewportWidth;
        if (scrollableLeft < scrollOffset_.x) {
            scrollOffset_.x = scrollableLeft;
        } else if (scrollableRight > scrollOffset_.x + visibleWidth) {
            scrollOffset_.x = scrollableRight - visibleWidth;
        }
    }
    if (data.height > 0.0F) {
        if (top < scrollOffset_.y) {
            scrollOffset_.y = top;
        } else if (bottom > scrollOffset_.y + data.height) {
            scrollOffset_.y = bottom - data.height;
        }
    }
    clampScrollOffset();
}

bool ListControl::hasVerticalScrollBar() const noexcept {
    return scrollGeometry().vertical;
}

bool ListControl::hasHorizontalScrollBar() const noexcept {
    return scrollGeometry().horizontal;
}

Rect ListControl::verticalScrollBarBounds() const noexcept {
    return scrollGeometry().verticalTrack;
}

Rect ListControl::verticalScrollThumbBounds() const noexcept {
    return scrollGeometry().verticalThumb;
}

Rect ListControl::horizontalScrollBarBounds() const noexcept {
    return scrollGeometry().horizontalTrack;
}

Rect ListControl::horizontalScrollThumbBounds() const noexcept {
    return scrollGeometry().horizontalThumb;
}

Rect ListControl::cellBounds(ListCellAddress address) const noexcept {
    if (!isValidAddress(address)) {
        return {};
    }
    const ScrollGeometry geometry = scrollGeometry();
    return {
        columnViewportX(address.column, geometry),
        geometry.data.y +
            static_cast<float>(address.row) * style_.rowHeight -
            scrollOffset_.y,
        columnWidth(address.column),
        style_.rowHeight,
    };
}

Rect ListControl::cellActionBounds(ListCellAddress address) const noexcept {
    const ListCell* value = cell(address);
    if (!value || value->kind == ListCellKind::Text) {
        return {};
    }
    Rect result = cellBounds(address);
    const float availableHeight = std::max(
        0.0F, result.height - style_.cellPadding.vertical());
    const float size = std::min(
        style_.controlSize, std::min(result.width, availableHeight));
    result.x = value->kind == ListCellKind::CheckBox
        ? result.x + style_.cellPadding.left
        : result.x + result.width - style_.cellPadding.right - size;
    result.y += std::max(0.0F, (result.height - size) * 0.5F);
    result.width = size;
    result.height = size;
    return result;
}

void ListControl::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        focused_ = false;
        hoveredCell_.reset();
        pressedCell_.reset();
        pointerPressed_ = false;
        hoveredScrollBarPart_ = ScrollBarPart::None;
        scrollDragAxis_ = ScrollDragAxis::None;
        hoveredHeaderColumn_.reset();
        hoveredResizeColumn_.reset();
        pressedHeaderColumn_.reset();
        resizingColumn_.reset();
        reorderingColumn_.reset();
        reorderTargetColumn_.reset();
        lastClickedCell_.reset();
        lastClickMilliseconds_ = 0;
    }
}

bool ListControl::isEnabled() const noexcept {
    return enabled_;
}

bool ListControl::isFocused() const noexcept {
    return focused_;
}

void ListControl::setPreferredSize(Size preferredSize) noexcept {
    preferredSize_ = preferredSize;
}

void ListControl::setOnSelectionChanged(
    SelectionChangedHandler handler) {
    onSelectionChanged_ = std::move(handler);
}

void ListControl::setOnCellAction(CellActionHandler handler) {
    onCellAction_ = std::move(handler);
}

void ListControl::setSortDescriptor(
    std::optional<ListSortDescriptor> descriptor) {
    if (descriptor) {
        if (descriptor->column >= columns_.size()) {
            throw std::out_of_range("ListControl sort column is out of range");
        }
        if (!columns_[descriptor->column].sortable) {
            throw std::invalid_argument("ListControl column is not sortable");
        }
    }
    sortDescriptor_ = descriptor;
}

std::optional<ListSortDescriptor> ListControl::sortDescriptor() const noexcept {
    return sortDescriptor_;
}

void ListControl::setOnSortChanged(SortChangedHandler handler) {
    onSortChanged_ = std::move(handler);
}

void ListControl::setOnColumnResized(ColumnResizedHandler handler) {
    onColumnResized_ = std::move(handler);
}

void ListControl::setOnColumnReordered(ColumnReorderedHandler handler) {
    onColumnReordered_ = std::move(handler);
}

void ListControl::setStyle(ListControlStyle style) noexcept {
    sanitize(style);
    style_ = style;
    invalidateLayouts();
    clampScrollOffset();
}

const ListControlStyle& ListControl::style() const noexcept {
    return style_;
}

void ListControl::onArrange() {
    clampScrollOffset();
    if (selectedCell_) {
        ensureCellVisible(*selectedCell_);
    }
}

void ListControl::onPaint(std::vector<PaintCommand>& commands) const {
    const float ringWidth = std::min(
        style_.focusRingWidth,
        std::min(bounds().width, bounds().height) * 0.5F);
    if (ringWidth > 0.0F) {
        commands.push_back({
            bounds(), clip(), focused_ ? style_.focusRing : style_.background,
            invalidTextureId,
            style_.cornerRadius});
    }

    const Rect content = contentBounds();
    const ScrollGeometry geometry = scrollGeometry();
    const Rect contentClip = intersect(content, clip());
    commands.push_back({
        content, contentClip, style_.background, invalidTextureId,
        std::max(0.0F, style_.cornerRadius - ringWidth)});
    if (!hasArea(contentClip)) {
        return;
    }

    const Rect header = geometry.header;
    const Rect headerClip = intersect(header, contentClip);
    commands.push_back({
        header, headerClip, style_.headerBackground, invalidTextureId, 0.0F});

    for (std::size_t column = 0; column < columns_.size(); ++column) {
        const Rect headerCell = headerCellBounds(column);
        const Rect cellClip = columnPaintClip(
            column, geometry, headerClip);
        const Rect visible = intersect(headerCell, cellClip);
        if (!hasArea(visible)) {
            continue;
        }
        if (pressedHeaderColumn_ == column) {
            commands.push_back({
                headerCell, cellClip, style_.headerPressed,
                invalidTextureId, 0.0F,
            });
        } else if (hoveredHeaderColumn_ == column) {
            commands.push_back({
                headerCell, cellClip, style_.headerHovered,
                invalidTextureId, 0.0F,
            });
        }
        Rect textBounds = headerCell;
        textBounds.x += style_.cellPadding.left;
        textBounds.width = std::max(
            0.0F, textBounds.width - style_.cellPadding.horizontal());
        if (sortDescriptor_ && sortDescriptor_->column == column) {
            textBounds.width = std::max(0.0F, textBounds.width - 14.0F);
        }
        paintText(
            rows_.size() * columns_.size() + column,
            columns_[column].title,
            textBounds,
            cellClip,
            style_.text,
            commands);
        if (style_.gridLineWidth > 0.0F) {
            commands.push_back({
                {
                    headerCell.x + headerCell.width - style_.gridLineWidth,
                    headerCell.y,
                    style_.gridLineWidth,
                    headerCell.height,
                },
                cellClip, style_.gridLine, invalidTextureId, 0.0F,
            });
        }
        paintSortIndicator(column, headerCell, cellClip, commands);
        if ((hoveredResizeColumn_ == column || resizingColumn_ == column) &&
            style_.columnResizeHandleWidth > 0.0F) {
            commands.push_back({
                {
                    headerCell.x + headerCell.width -
                        style_.columnResizeHandleWidth * 0.5F,
                    headerCell.y,
                    style_.columnResizeHandleWidth,
                    headerCell.height,
                },
                cellClip, style_.columnResizeHandle,
                invalidTextureId, 0.0F,
            });
        }
    }

    const Rect data = geometry.data;
    const Rect dataClip = intersect(data, contentClip);
    if (!hasArea(dataClip) || rows_.empty() || columns_.empty()) {
        paintScrollBars(geometry, contentClip, commands);
        paintColumnGuides(geometry, contentClip, commands);
        return;
    }

    const std::size_t firstRow = std::min(
        rows_.size(),
        static_cast<std::size_t>(scrollOffset_.y / style_.rowHeight));
    const std::size_t visibleRows = static_cast<std::size_t>(
        std::ceil(data.height / style_.rowHeight)) + 1;
    const std::size_t endRow = std::min(
        rows_.size(), firstRow + visibleRows);

    for (std::size_t row = firstRow; row < endRow; ++row) {
        const float rowY = data.y + static_cast<float>(row) *
            style_.rowHeight - scrollOffset_.y;
        if ((row % 2U) != 0U) {
            commands.push_back({
                {data.x, rowY, data.width, style_.rowHeight},
                dataClip, style_.alternateRow, invalidTextureId, 0.0F,
            });
        }

        for (std::size_t column = 0; column < columns_.size(); ++column) {
            const ListCellAddress address{row, column};
            const Rect cellRectangle = cellBounds(address);
            const Rect cellClip = columnPaintClip(
                column, geometry, dataClip);
            const Rect visible = intersect(cellRectangle, cellClip);
            if (!hasArea(visible)) {
                continue;
            }
            if (selectedCell_ == address) {
                commands.push_back({
                    cellRectangle,
                    cellClip,
                    focused_ ? style_.selectedCellFocused : style_.selectedCell,
                    invalidTextureId,
                    0.0F,
                });
            } else if (hoveredCell_ == address) {
                commands.push_back({
                    cellRectangle, cellClip, style_.hoveredCell,
                    invalidTextureId, 0.0F,
                });
            }

            const ListCell& value = rows_[row][column];
            Rect textBounds = cellRectangle;
            textBounds.x += style_.cellPadding.left;
            textBounds.width = std::max(
                0.0F, textBounds.width - style_.cellPadding.horizontal());
            if (value.kind == ListCellKind::CheckBox) {
                const Rect action = cellActionBounds(address);
                textBounds.x = action.x + action.width + style_.cellPadding.left;
                textBounds.width = std::max(
                    0.0F,
                    cellRectangle.x + cellRectangle.width -
                        style_.cellPadding.right - textBounds.x);
            } else if (value.kind != ListCellKind::Text) {
                textBounds.width = std::max(
                    0.0F,
                    textBounds.width - style_.controlSize -
                        style_.cellPadding.left);
            }
            paintText(
                cacheIndex(row, column),
                displayText(value),
                textBounds,
                cellClip,
                value.editable ? style_.text : style_.secondaryText,
                commands);
            paintCellControl(value, cellRectangle, cellClip, commands);

            if (style_.gridLineWidth > 0.0F) {
                commands.push_back({
                    {
                        cellRectangle.x + cellRectangle.width -
                            style_.gridLineWidth,
                        cellRectangle.y,
                        style_.gridLineWidth,
                        cellRectangle.height,
                    },
                    cellClip, style_.gridLine, invalidTextureId, 0.0F,
                });
            }
        }
        if (style_.gridLineWidth > 0.0F) {
            commands.push_back({
                {
                    data.x,
                    rowY + style_.rowHeight - style_.gridLineWidth,
                    data.width,
                    style_.gridLineWidth,
                },
                dataClip, style_.gridLine, invalidTextureId, 0.0F,
            });
        }
    }
    paintScrollBars(geometry, contentClip, commands);
    paintColumnGuides(geometry, contentClip, commands);
}

bool ListControl::acceptsPointerEvents() const noexcept {
    return enabled_;
}

bool ListControl::onPointerEvent(const WidgetPointerEvent& event) {
    if (!enabled_) {
        return false;
    }
    switch (event.type) {
    case WidgetPointerEventType::Enter:
    case WidgetPointerEventType::Move: {
        bool changed = updateScrollBarHover(event.position);
        changed = dragScrollBar(event.position) || changed;
        changed = updateHeaderHover(event.position) || changed;
        changed = dragColumnResize(event.position) || changed;
        changed = dragColumnReorder(event.position) || changed;
        const auto next = event.inside
            ? addressAt(event.position)
            : std::nullopt;
        changed = next != hoveredCell_ || changed;
        hoveredCell_ = next;
        return changed;
    }
    case WidgetPointerEventType::Leave: {
        const bool changed = hoveredCell_.has_value();
        hoveredCell_.reset();
        const bool scrollChanged =
            hoveredScrollBarPart_ != ScrollBarPart::None;
        hoveredScrollBarPart_ = ScrollBarPart::None;
        const bool headerChanged = hoveredHeaderColumn_.has_value() ||
            hoveredResizeColumn_.has_value();
        hoveredHeaderColumn_.reset();
        hoveredResizeColumn_.reset();
        return changed || scrollChanged || headerChanged;
    }
    case WidgetPointerEventType::Press:
        if (event.button != PointerButton::Primary) {
            return false;
        }
        hoveredScrollBarPart_ = scrollBarPartAt(event.position);
        if (hoveredScrollBarPart_ != ScrollBarPart::None) {
            pointerPressed_ = false;
            pressedCell_.reset();
            pressedHeaderColumn_.reset();
            reorderingColumn_.reset();
            reorderTargetColumn_.reset();
            lastClickedCell_.reset();
            lastClickMilliseconds_ = 0;
            if (hoveredScrollBarPart_ == ScrollBarPart::VerticalThumb) {
                scrollDragAxis_ = ScrollDragAxis::Vertical;
                scrollDragPointerStart_ = event.position.y;
                scrollDragOffsetStart_ = scrollOffset_.y;
            } else if (
                hoveredScrollBarPart_ == ScrollBarPart::HorizontalThumb) {
                scrollDragAxis_ = ScrollDragAxis::Horizontal;
                scrollDragPointerStart_ = event.position.x;
                scrollDragOffsetStart_ = scrollOffset_.x;
            } else {
                pageScrollBar(hoveredScrollBarPart_, event.position);
            }
            return true;
        }
        hoveredResizeColumn_ = resizeColumnAt(event.position);
        if (hoveredResizeColumn_) {
            resizingColumn_ = hoveredResizeColumn_;
            columnResizePointerStart_ = event.position.x;
            columnResizeWidthStart_ = columnWidth(*resizingColumn_);
            pressedHeaderColumn_.reset();
            reorderingColumn_.reset();
            reorderTargetColumn_.reset();
            pointerPressed_ = false;
            pressedCell_.reset();
            return true;
        }
        hoveredHeaderColumn_ = headerColumnAt(event.position);
        if (hoveredHeaderColumn_) {
            pressedHeaderColumn_ = hoveredHeaderColumn_;
            columnReorderPointerStart_ = event.position.x;
            reorderingColumn_.reset();
            reorderTargetColumn_.reset();
            pointerPressed_ = false;
            pressedCell_.reset();
            lastClickedCell_.reset();
            lastClickMilliseconds_ = 0;
            return true;
        }
        pointerPressed_ = true;
        pressedCell_ = addressAt(event.position);
        select(pressedCell_, true);
        return true;
    case WidgetPointerEventType::Release: {
        if (event.button == PointerButton::Primary && resizingColumn_) {
            dragColumnResize(event.position);
            resizingColumn_.reset();
            updateHeaderHover(event.position);
            return true;
        }
        if (event.button == PointerButton::Primary && reorderingColumn_) {
            const std::size_t from = *reorderingColumn_;
            const std::size_t to = reorderTargetColumn_.value_or(from);
            reorderingColumn_.reset();
            reorderTargetColumn_.reset();
            pressedHeaderColumn_.reset();
            if (from != to) {
                moveColumn(from, to);
                if (onColumnReordered_) {
                    onColumnReordered_(from, to);
                }
            }
            updateHeaderHover(event.position);
            return true;
        }
        if (event.button == PointerButton::Primary &&
            scrollDragAxis_ != ScrollDragAxis::None) {
            dragScrollBar(event.position);
            scrollDragAxis_ = ScrollDragAxis::None;
            updateScrollBarHover(event.position);
            return true;
        }
        if (event.button == PointerButton::Primary && pressedHeaderColumn_) {
            const std::optional<std::size_t> releasedHeader =
                headerColumnAt(event.position);
            const std::size_t column = *pressedHeaderColumn_;
            pressedHeaderColumn_.reset();
            if (releasedHeader == column) {
                toggleSort(column);
            }
            updateHeaderHover(event.position);
            return true;
        }
        if (event.button != PointerButton::Primary || !pointerPressed_) {
            return false;
        }
        pointerPressed_ = false;
        const auto releasedCell = event.inside
            ? addressAt(event.position)
            : std::nullopt;
        const bool activate = pressedCell_ && releasedCell == pressedCell_;
        const auto address = pressedCell_;
        pressedCell_.reset();
        if (activate && address) {
            const ListCell* value = cell(*address);
            if (value != nullptr) {
                if (value->kind == ListCellKind::Text) {
                    const std::uint64_t now = monotonicMilliseconds();
                    const bool doubleClick = lastClickedCell_ == address &&
                        now >= lastClickMilliseconds_ &&
                        now - lastClickMilliseconds_ <=
                            doubleClickIntervalMilliseconds;
                    lastClickedCell_ = address;
                    lastClickMilliseconds_ = now;
                    if (doubleClick) {
                        lastClickedCell_.reset();
                        lastClickMilliseconds_ = 0;
                        activateCell(*address, true);
                    }
                } else {
                    lastClickedCell_.reset();
                    lastClickMilliseconds_ = 0;
                    if (contains(
                        cellActionBounds(*address), event.position)) {
                        activateCell(*address, false);
                    }
                }
            }
        } else {
            lastClickedCell_.reset();
            lastClickMilliseconds_ = 0;
        }
        return true;
    }
    case WidgetPointerEventType::Cancel:
        if (!pointerPressed_ && !hoveredCell_ &&
            scrollDragAxis_ == ScrollDragAxis::None &&
            hoveredScrollBarPart_ == ScrollBarPart::None &&
            !hoveredHeaderColumn_ && !hoveredResizeColumn_ &&
            !pressedHeaderColumn_ && !resizingColumn_ &&
            !reorderingColumn_ && !reorderTargetColumn_) {
            return false;
        }
        pointerPressed_ = false;
        pressedCell_.reset();
        hoveredCell_.reset();
        hoveredScrollBarPart_ = ScrollBarPart::None;
        scrollDragAxis_ = ScrollDragAxis::None;
        hoveredHeaderColumn_.reset();
        hoveredResizeColumn_.reset();
        pressedHeaderColumn_.reset();
        resizingColumn_.reset();
        reorderingColumn_.reset();
        reorderTargetColumn_.reset();
        lastClickedCell_.reset();
        lastClickMilliseconds_ = 0;
        return true;
    }
    return false;
}

bool ListControl::acceptsFocus() const noexcept {
    return enabled_;
}

bool ListControl::onFocusChanged(bool focused) {
    const bool changed = focused_ != focused;
    focused_ = focused;
    return changed;
}

bool ListControl::onKeyEvent(const WidgetKeyEvent& event) {
    if (!enabled_) {
        return false;
    }
    const bool navigationKey =
        event.key == KeyCode::Left || event.key == KeyCode::Right ||
        event.key == KeyCode::Up || event.key == KeyCode::Down ||
        event.key == KeyCode::Home || event.key == KeyCode::End ||
        event.key == KeyCode::PageUp || event.key == KeyCode::PageDown;
    const bool activationKey =
        event.key == KeyCode::Enter || event.key == KeyCode::Space;
    if (!navigationKey && !activationKey) {
        return false;
    }
    if (event.type != WidgetKeyEventType::Press) {
        return true;
    }

    switch (event.key) {
    case KeyCode::Left:
        moveSelection(0, -1);
        break;
    case KeyCode::Right:
        moveSelection(0, 1);
        break;
    case KeyCode::Up:
        moveSelection(-1, 0);
        break;
    case KeyCode::Down:
        moveSelection(1, 0);
        break;
    case KeyCode::Home:
        if (!rows_.empty() && !columns_.empty()) {
            const std::size_t row = event.modifiers.control || !selectedCell_
                ? 0 : selectedCell_->row;
            select(ListCellAddress{row, 0}, true);
            ensureCellVisible(*selectedCell_);
        }
        break;
    case KeyCode::End:
        if (!rows_.empty() && !columns_.empty()) {
            const std::size_t row = event.modifiers.control || !selectedCell_
                ? rows_.size() - 1 : selectedCell_->row;
            select(ListCellAddress{row, columns_.size() - 1}, true);
            ensureCellVisible(*selectedCell_);
        }
        break;
    case KeyCode::PageUp:
    case KeyCode::PageDown: {
        const Rect data = scrollGeometry().data;
        const int pageRows = std::max(
            1, static_cast<int>(data.height / style_.rowHeight));
        moveSelection(
            event.key == KeyCode::PageUp ? -pageRows : pageRows, 0);
        break;
    }
    case KeyCode::Enter:
        if (selectedCell_) {
            activateCell(*selectedCell_, true);
        }
        break;
    case KeyCode::Space:
        if (selectedCell_) {
            activateCell(*selectedCell_, false);
        }
        break;
    default:
        break;
    }
    return true;
}

bool ListControl::onScrollEvent(const WidgetScrollEvent& event) {
    if (!enabled_) {
        return false;
    }
    const float unit = event.mode == ScrollDeltaMode::Line
        ? style_.rowHeight
        : 1.0F;
    const Point delta{
        std::isfinite(event.delta.x) ? event.delta.x * unit : 0.0F,
        std::isfinite(event.delta.y) ? event.delta.y * unit : 0.0F,
    };
    return scrollBy(delta);
}

Rect ListControl::contentBounds() const noexcept {
    return inset(bounds(), style_.focusRingWidth);
}

ListControl::ScrollGeometry ListControl::scrollGeometry() const noexcept {
    ScrollGeometry result;
    const Rect content = contentBounds();
    const Size size = contentSize();
    const float frozenContentWidth = frozenColumnsWidth();
    const float scrollableContentWidth = std::max(
        0.0F, size.width - frozenContentWidth);
    const float thickness = std::min(
        style_.scrollBarThickness,
        std::min(content.width, content.height));

    for (int pass = 0; pass < 3; ++pass) {
        const float width = std::max(
            0.0F, content.width - (result.vertical ? thickness : 0.0F));
        const float height = std::max(
            0.0F, content.height - (result.horizontal ? thickness : 0.0F));
        const float headerHeight = std::min(style_.headerHeight, height);
        const float frozenVisibleWidth = std::min(
            frozenContentWidth, width);
        const float scrollableViewportWidth = std::max(
            0.0F, width - frozenVisibleWidth);
        result.horizontal = result.horizontal ||
            scrollableContentWidth > scrollableViewportWidth;
        result.vertical = result.vertical ||
            size.height > std::max(0.0F, height - headerHeight);
    }

    result.viewport = {
        content.x,
        content.y,
        std::max(
            0.0F,
            content.width - (result.vertical ? thickness : 0.0F)),
        std::max(
            0.0F,
            content.height - (result.horizontal ? thickness : 0.0F)),
    };
    const float headerHeight = std::min(
        style_.headerHeight, result.viewport.height);
    result.header = {
        result.viewport.x,
        result.viewport.y,
        result.viewport.width,
        headerHeight,
    };
    result.data = {
        result.viewport.x,
        result.viewport.y + headerHeight,
        result.viewport.width,
        std::max(0.0F, result.viewport.height - headerHeight),
    };
    result.frozenWidth = std::min(
        frozenContentWidth, result.viewport.width);
    result.scrollableViewportWidth = std::max(
        0.0F, result.viewport.width - result.frozenWidth);
    result.scrollableContentWidth = scrollableContentWidth;

    if (result.vertical) {
        result.verticalTrack = {
            result.viewport.x + result.viewport.width,
            result.data.y,
            thickness,
            result.data.height,
        };
        const float trackLength = result.verticalTrack.height;
        const float viewportLength = result.data.height;
        const float contentLength = size.height;
        const float minimumLength = std::min(
            style_.minimumScrollThumbLength, trackLength);
        const float thumbLength = contentLength > 0.0F
            ? std::clamp(
                trackLength * viewportLength / contentLength,
                minimumLength,
                trackLength)
            : trackLength;
        const float maximumOffset = std::max(
            0.0F, contentLength - viewportLength);
        const float travel = std::max(0.0F, trackLength - thumbLength);
        const float thumbOffset = maximumOffset > 0.0F
            ? std::clamp(scrollOffset_.y, 0.0F, maximumOffset) /
                maximumOffset * travel
            : 0.0F;
        result.verticalThumb = {
            result.verticalTrack.x,
            result.verticalTrack.y + thumbOffset,
            result.verticalTrack.width,
            thumbLength,
        };
    }

    if (result.horizontal) {
        result.horizontalTrack = {
            result.viewport.x + result.frozenWidth,
            result.viewport.y + result.viewport.height,
            result.scrollableViewportWidth,
            thickness,
        };
        const float trackLength = result.horizontalTrack.width;
        const float viewportLength = result.scrollableViewportWidth;
        const float contentLength = result.scrollableContentWidth;
        const float minimumLength = std::min(
            style_.minimumScrollThumbLength, trackLength);
        const float thumbLength = contentLength > 0.0F
            ? std::clamp(
                trackLength * viewportLength / contentLength,
                minimumLength,
                trackLength)
            : trackLength;
        const float maximumOffset = std::max(
            0.0F, contentLength - viewportLength);
        const float travel = std::max(0.0F, trackLength - thumbLength);
        const float thumbOffset = maximumOffset > 0.0F
            ? std::clamp(scrollOffset_.x, 0.0F, maximumOffset) /
                maximumOffset * travel
            : 0.0F;
        result.horizontalThumb = {
            result.horizontalTrack.x + thumbOffset,
            result.horizontalTrack.y,
            thumbLength,
            result.horizontalTrack.height,
        };
    }

    if (result.vertical && result.horizontal) {
        result.corner = {
            result.viewport.x + result.viewport.width,
            result.viewport.y + result.viewport.height,
            thickness,
            thickness,
        };
    }
    return result;
}

ListControl::ScrollBarPart ListControl::scrollBarPartAt(
    Point position) const noexcept {
    const ScrollGeometry geometry = scrollGeometry();
    if (geometry.vertical && contains(geometry.verticalThumb, position)) {
        return ScrollBarPart::VerticalThumb;
    }
    if (geometry.horizontal && contains(geometry.horizontalThumb, position)) {
        return ScrollBarPart::HorizontalThumb;
    }
    if (geometry.vertical && contains(geometry.verticalTrack, position)) {
        return ScrollBarPart::VerticalTrack;
    }
    if (geometry.horizontal && contains(geometry.horizontalTrack, position)) {
        return ScrollBarPart::HorizontalTrack;
    }
    if (hasArea(geometry.corner) && contains(geometry.corner, position)) {
        return ScrollBarPart::Corner;
    }
    return ScrollBarPart::None;
}

bool ListControl::updateScrollBarHover(Point position) noexcept {
    const ScrollBarPart next = scrollBarPartAt(position);
    const bool changed = next != hoveredScrollBarPart_;
    hoveredScrollBarPart_ = next;
    return changed;
}

bool ListControl::dragScrollBar(Point position) noexcept {
    if (scrollDragAxis_ == ScrollDragAxis::None) {
        return false;
    }
    const ScrollGeometry geometry = scrollGeometry();
    const Size size = contentSize();
    Point next = scrollOffset_;
    if (scrollDragAxis_ == ScrollDragAxis::Vertical) {
        const float travel = std::max(
            0.0F,
            geometry.verticalTrack.height - geometry.verticalThumb.height);
        const float maximumOffset = std::max(
            0.0F, size.height - geometry.data.height);
        if (travel > 0.0F && maximumOffset > 0.0F) {
            next.y = scrollDragOffsetStart_ +
                (position.y - scrollDragPointerStart_) /
                    travel * maximumOffset;
        }
    } else {
        const float travel = std::max(
            0.0F,
            geometry.horizontalTrack.width - geometry.horizontalThumb.width);
        const float maximumOffset = std::max(
            0.0F,
            geometry.scrollableContentWidth -
                geometry.scrollableViewportWidth);
        if (travel > 0.0F && maximumOffset > 0.0F) {
            next.x = scrollDragOffsetStart_ +
                (position.x - scrollDragPointerStart_) /
                    travel * maximumOffset;
        }
    }
    const Point before = scrollOffset_;
    setScrollOffset(next);
    return before.x != scrollOffset_.x || before.y != scrollOffset_.y;
}

bool ListControl::pageScrollBar(
    ScrollBarPart part,
    Point position) noexcept {
    const ScrollGeometry geometry = scrollGeometry();
    Point delta{};
    if (part == ScrollBarPart::VerticalTrack) {
        delta.y = position.y < geometry.verticalThumb.y
            ? -geometry.data.height
            : geometry.data.height;
    } else if (part == ScrollBarPart::HorizontalTrack) {
        delta.x = position.x < geometry.horizontalThumb.x
            ? -geometry.scrollableViewportWidth
            : geometry.scrollableViewportWidth;
    } else {
        return false;
    }
    return scrollBy(delta);
}

bool ListControl::scrollBy(Point delta) noexcept {
    const Point before = scrollOffset_;
    setScrollOffset({
        scrollOffset_.x + delta.x,
        scrollOffset_.y + delta.y,
    });
    return before.x != scrollOffset_.x || before.y != scrollOffset_.y;
}

void ListControl::paintScrollBars(
    const ScrollGeometry& geometry,
    Rect paintClip,
    std::vector<PaintCommand>& commands) const {
    const float radius = style_.scrollBarThickness * 0.5F;
    if (geometry.vertical) {
        commands.push_back({
            geometry.verticalTrack, paintClip, style_.scrollBarTrack,
            invalidTextureId, radius,
        });
        const Color thumbColor =
            scrollDragAxis_ == ScrollDragAxis::Vertical
            ? style_.scrollBarThumbPressed
            : (hoveredScrollBarPart_ == ScrollBarPart::VerticalThumb
                ? style_.scrollBarThumbHovered
                : style_.scrollBarThumb);
        commands.push_back({
            geometry.verticalThumb, paintClip, thumbColor,
            invalidTextureId, radius,
        });
    }
    if (geometry.horizontal) {
        commands.push_back({
            geometry.horizontalTrack, paintClip, style_.scrollBarTrack,
            invalidTextureId, radius,
        });
        const Color thumbColor =
            scrollDragAxis_ == ScrollDragAxis::Horizontal
            ? style_.scrollBarThumbPressed
            : (hoveredScrollBarPart_ == ScrollBarPart::HorizontalThumb
                ? style_.scrollBarThumbHovered
                : style_.scrollBarThumb);
        commands.push_back({
            geometry.horizontalThumb, paintClip, thumbColor,
            invalidTextureId, radius,
        });
    }
    if (hasArea(geometry.corner)) {
        commands.push_back({
            geometry.corner, paintClip, style_.scrollBarTrack,
            invalidTextureId, 0.0F,
        });
    }
}

std::optional<std::size_t> ListControl::headerColumnAt(
    Point position) const noexcept {
    const ScrollGeometry geometry = scrollGeometry();
    if (!contains(geometry.header, position) || columns_.empty()) {
        return std::nullopt;
    }
    const bool frozen = position.x <
        geometry.header.x + geometry.frozenWidth;
    const float contentX = position.x - geometry.header.x +
        (frozen ? 0.0F : scrollOffset_.x);
    float x = 0.0F;
    for (std::size_t column = 0; column < columns_.size(); ++column) {
        x += columns_[column].width;
        if (contentX < x) {
            return column;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ListControl::resizeColumnAt(
    Point position) const noexcept {
    const ScrollGeometry geometry = scrollGeometry();
    if (!contains(geometry.header, position)) {
        return std::nullopt;
    }
    for (std::size_t column = 0; column < columns_.size(); ++column) {
        if (!columns_[column].resizable) {
            continue;
        }
        const Rect handle = intersect(
            columnResizeHandleBounds(column),
            columnPaintClip(column, geometry, geometry.header));
        if (hasArea(handle) && contains(handle, position)) {
            return column;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ListControl::reorderTargetAt(
    Point position) const noexcept {
    if (columns_.empty()) {
        return std::nullopt;
    }
    const ScrollGeometry geometry = scrollGeometry();
    const float clampedX = std::clamp(
        position.x,
        geometry.header.x,
        geometry.header.x + geometry.header.width);
    const bool frozen = clampedX <
        geometry.header.x + geometry.frozenWidth;
    const float contentX = clampedX - geometry.header.x +
        (frozen ? 0.0F : scrollOffset_.x);
    float x = 0.0F;
    for (std::size_t column = 0; column < columns_.size(); ++column) {
        const float center = x + columns_[column].width * 0.5F;
        if (contentX < center) {
            return column;
        }
        x += columns_[column].width;
    }
    return columns_.size() - 1;
}

bool ListControl::updateHeaderHover(Point position) noexcept {
    const auto resize = resizeColumnAt(position);
    const auto header = resize ? std::optional<std::size_t>{}
                               : headerColumnAt(position);
    const bool changed = resize != hoveredResizeColumn_ ||
        header != hoveredHeaderColumn_;
    hoveredResizeColumn_ = resize;
    hoveredHeaderColumn_ = header;
    return changed;
}

bool ListControl::dragColumnResize(Point position) {
    if (!resizingColumn_ || *resizingColumn_ >= columns_.size()) {
        return false;
    }
    const std::size_t column = *resizingColumn_;
    const float previous = columnWidth(column);
    setColumnWidth(
        column,
        columnResizeWidthStart_ + position.x - columnResizePointerStart_);
    const float current = columnWidth(column);
    if (previous == current) {
        return false;
    }
    if (onColumnResized_) {
        onColumnResized_(column, current);
    }
    return true;
}

bool ListControl::dragColumnReorder(Point position) noexcept {
    if (!pressedHeaderColumn_ || resizingColumn_ ||
        *pressedHeaderColumn_ >= columns_.size() ||
        !columns_[*pressedHeaderColumn_].reorderable) {
        return false;
    }
    if (!reorderingColumn_ &&
        std::abs(position.x - columnReorderPointerStart_) <
            style_.columnReorderDragThreshold) {
        return false;
    }
    const std::size_t source = *pressedHeaderColumn_;
    const std::optional<std::size_t> target = reorderTargetAt(position);
    const bool changed = !reorderingColumn_ ||
        target != reorderTargetColumn_;
    reorderingColumn_ = source;
    reorderTargetColumn_ = target.value_or(source);
    return changed;
}

bool ListControl::toggleSort(std::size_t column) {
    if (column >= columns_.size() || !columns_[column].sortable) {
        return false;
    }
    const ListSortDirection direction =
        sortDescriptor_ && sortDescriptor_->column == column &&
            sortDescriptor_->direction == ListSortDirection::Ascending
        ? ListSortDirection::Descending
        : ListSortDirection::Ascending;
    sortDescriptor_ = ListSortDescriptor{column, direction};
    if (onSortChanged_) {
        onSortChanged_(sortDescriptor_);
    }
    return true;
}

void ListControl::paintSortIndicator(
    std::size_t column,
    Rect headerCell,
    Rect paintClip,
    std::vector<PaintCommand>& commands) const {
    if (!sortDescriptor_ || sortDescriptor_->column != column ||
        headerCell.width < 12.0F || headerCell.height < 8.0F) {
        return;
    }
    constexpr float widths[]{3.0F, 6.0F, 9.0F};
    const float lineHeight = std::max(1.0F, headerCell.height * 0.06F);
    const float gap = std::max(1.0F, lineHeight * 0.75F);
    const float totalHeight = lineHeight * 3.0F + gap * 2.0F;
    const float centerX = headerCell.x + headerCell.width - 9.0F;
    const float startY = headerCell.y +
        std::max(0.0F, (headerCell.height - totalHeight) * 0.5F);
    for (std::size_t index = 0; index < 3; ++index) {
        const std::size_t widthIndex =
            sortDescriptor_->direction == ListSortDirection::Ascending
            ? index
            : 2U - index;
        const float width = widths[widthIndex];
        commands.push_back({
            {
                centerX - width * 0.5F,
                startY + static_cast<float>(index) * (lineHeight + gap),
                width,
                lineHeight,
            },
            paintClip, style_.sortIndicator, invalidTextureId,
            lineHeight * 0.5F,
        });
    }
}

void ListControl::paintColumnGuides(
    const ScrollGeometry& geometry,
    Rect paintClip,
    std::vector<PaintCommand>& commands) const {
    if (frozenColumnCount_ > 0 && geometry.frozenWidth > 0.0F &&
        style_.frozenColumnDividerWidth > 0.0F) {
        commands.push_back({
            {
                geometry.viewport.x + geometry.frozenWidth -
                    style_.frozenColumnDividerWidth * 0.5F,
                geometry.viewport.y,
                style_.frozenColumnDividerWidth,
                geometry.viewport.height,
            },
            intersect(geometry.viewport, paintClip),
            style_.frozenColumnDivider,
            invalidTextureId,
            0.0F,
        });
    }
    if (!reorderingColumn_ || !reorderTargetColumn_ ||
        *reorderingColumn_ == *reorderTargetColumn_ ||
        style_.columnReorderIndicatorWidth <= 0.0F) {
        return;
    }
    const std::size_t source = *reorderingColumn_;
    const std::size_t target = *reorderTargetColumn_;
    const Rect targetBounds = headerCellBounds(target);
    const float x = target < source
        ? targetBounds.x
        : targetBounds.x + targetBounds.width;
    commands.push_back({
        {
            x - style_.columnReorderIndicatorWidth * 0.5F,
            geometry.viewport.y,
            style_.columnReorderIndicatorWidth,
            geometry.viewport.height,
        },
        intersect(geometry.viewport, paintClip),
        style_.columnReorderIndicator,
        invalidTextureId,
        style_.columnReorderIndicatorWidth * 0.5F,
    });
}

std::optional<ListCellAddress> ListControl::addressAt(
    Point position) const noexcept {
    const ScrollGeometry geometry = scrollGeometry();
    const Rect data = geometry.data;
    if (!contains(data, position) || rows_.empty() || columns_.empty()) {
        return std::nullopt;
    }
    const bool frozen = position.x < data.x + geometry.frozenWidth;
    const float contentX = position.x - data.x +
        (frozen ? 0.0F : scrollOffset_.x);
    const float contentY = position.y - data.y + scrollOffset_.y;
    const std::size_t row = static_cast<std::size_t>(
        contentY / style_.rowHeight);
    if (row >= rows_.size()) {
        return std::nullopt;
    }
    float x = 0.0F;
    for (std::size_t column = 0; column < columns_.size(); ++column) {
        x += columns_[column].width;
        if (contentX < x) {
            return ListCellAddress{row, column};
        }
    }
    return std::nullopt;
}

bool ListControl::isValidAddress(ListCellAddress address) const noexcept {
    return address.row < rows_.size() &&
        address.column < columns_.size() &&
        address.column < rows_[address.row].size();
}

float ListControl::columnStart(std::size_t column) const noexcept {
    float result = 0.0F;
    const std::size_t end = std::min(column, columns_.size());
    for (std::size_t index = 0; index < end; ++index) {
        result += columns_[index].width;
    }
    return result;
}

float ListControl::frozenColumnsWidth() const noexcept {
    return columnStart(std::min(frozenColumnCount_, columns_.size()));
}

float ListControl::columnViewportX(
    std::size_t column,
    const ScrollGeometry& geometry) const noexcept {
    return geometry.viewport.x + columnStart(column) -
        (column < frozenColumnCount_ ? 0.0F : scrollOffset_.x);
}

Rect ListControl::columnPaintClip(
    std::size_t column,
    const ScrollGeometry& geometry,
    Rect baseClip) const noexcept {
    const bool frozen = column < frozenColumnCount_;
    const Rect region{
        geometry.viewport.x + (frozen ? 0.0F : geometry.frozenWidth),
        geometry.viewport.y,
        frozen ? geometry.frozenWidth : geometry.scrollableViewportWidth,
        geometry.viewport.height,
    };
    return intersect(baseClip, region);
}

std::size_t ListControl::remapColumnIndex(
    std::size_t index,
    std::size_t from,
    std::size_t to) noexcept {
    if (index == from) {
        return to;
    }
    if (from < to && index > from && index <= to) {
        return index - 1;
    }
    if (to < from && index >= to && index < from) {
        return index + 1;
    }
    return index;
}

void ListControl::clampScrollOffset() noexcept {
    const Size size = contentSize();
    const ScrollGeometry geometry = scrollGeometry();
    const Rect data = geometry.data;
    scrollOffset_.x = std::clamp(
        dimension(scrollOffset_.x),
        0.0F,
        std::max(
            0.0F,
            geometry.scrollableContentWidth -
                geometry.scrollableViewportWidth));
    scrollOffset_.y = std::clamp(
        dimension(scrollOffset_.y),
        0.0F,
        std::max(0.0F, size.height - data.height));
}

bool ListControl::select(
    std::optional<ListCellAddress> address,
    bool notify) {
    if (selectedCell_ == address) {
        return false;
    }
    selectedCell_ = address;
    if (notify && onSelectionChanged_) {
        SelectionChangedHandler callback = onSelectionChanged_;
        callback(selectedCell_);
    }
    return true;
}

bool ListControl::moveSelection(int rowDelta, int columnDelta) {
    if (rows_.empty() || columns_.empty()) {
        return false;
    }
    int row = selectedCell_ ? static_cast<int>(selectedCell_->row) : 0;
    int column = selectedCell_
        ? static_cast<int>(selectedCell_->column) : 0;
    if (selectedCell_) {
        row += rowDelta;
        column += columnDelta;
    }
    row = std::clamp(row, 0, static_cast<int>(rows_.size()) - 1);
    column = std::clamp(
        column, 0, static_cast<int>(columns_.size()) - 1);
    const ListCellAddress address{
        static_cast<std::size_t>(row),
        static_cast<std::size_t>(column),
    };
    const bool changed = select(address, true);
    ensureCellVisible(address);
    return changed;
}

bool ListControl::activateCell(
    ListCellAddress address,
    bool editText) {
    if (!isValidAddress(address)) {
        return false;
    }
    ListCell& value = rows_[address.row][address.column];
    if (!value.editable && value.kind != ListCellKind::ActionButton) {
        notifyCellAction(address, ListCellAction::Activate);
        return true;
    }
    const ListCellAction action = defaultAction(value, editText);
    if (action == ListCellAction::ToggleCheck) {
        value.checked = !value.checked;
    }
    notifyCellAction(address, action);
    return true;
}

ListCellAction ListControl::defaultAction(
    const ListCell& value,
    bool editText) const noexcept {
    switch (value.kind) {
    case ListCellKind::Text:
        return editText
            ? ListCellAction::BeginEdit
            : ListCellAction::Activate;
    case ListCellKind::CheckBox:
        return ListCellAction::ToggleCheck;
    case ListCellKind::ComboBox:
        return ListCellAction::OpenComboBox;
    case ListCellKind::Lookup:
        return ListCellAction::OpenLookup;
    case ListCellKind::ActionButton:
        return ListCellAction::InvokeButton;
    }
    return ListCellAction::Activate;
}

void ListControl::notifyCellAction(
    ListCellAddress address,
    ListCellAction action) {
    if (!onCellAction_) {
        return;
    }
    CellActionHandler callback = onCellAction_;
    Rect anchor = action == ListCellAction::OpenComboBox ||
            action == ListCellAction::OpenLookup
        ? cellBounds(address)
        : cellActionBounds(address);
    if (!hasArea(anchor)) {
        anchor = cellBounds(address);
    }
    callback({address, action, anchor});
}

void ListControl::invalidateLayouts() const noexcept {
    layouts_.clear();
}

const TextLayout& ListControl::ensureLayout(
    std::size_t index,
    std::string_view text,
    float maximumWidth) const {
    if (layouts_.size() <= index) {
        layouts_.resize(index + 1);
    }
    LayoutCache& cached = layouts_[index];
    const float normalizedWidth = std::isfinite(maximumWidth)
        ? std::max(0.0F, maximumWidth)
        : unboundedLayoutSize;
    if (!cached.layout || cached.text != text ||
        cached.maximumWidth != normalizedWidth) {
        TextLayoutOptions options;
        options.maximumWidth = normalizedWidth;
        options.maximumLines = 1;
        options.wrap = false;
        cached.text = std::string(text);
        cached.maximumWidth = normalizedWidth;
        cached.layout = textEngine_->createLayout(
            text, textStyle_, options);
        if (!cached.layout) {
            throw std::runtime_error(
                "ListControl TextEngine returned a null layout");
        }
    }
    return *cached.layout;
}

std::size_t ListControl::cacheIndex(
    std::size_t row,
    std::size_t column) const noexcept {
    return row * columns_.size() + column;
}

std::string ListControl::displayText(const ListCell& value) const {
    if (value.kind == ListCellKind::ComboBox && !value.options.empty()) {
        return value.options[std::min(
            value.selectedOption, value.options.size() - 1)];
    }
    return value.text;
}

void ListControl::paintText(
    std::size_t index,
    std::string_view text,
    Rect textBounds,
    Rect paintClip,
    Color color,
    std::vector<PaintCommand>& commands) const {
    if (text.empty() || !hasArea(textBounds)) {
        return;
    }
    const Rect textClip = intersect(textBounds, intersect(paintClip, clip()));
    if (!hasArea(textClip)) {
        return;
    }
    const TextLayout& layout = ensureLayout(index, text, textBounds.width);
    const Size size = layout.size();
    layout.appendPaintCommands(
        {
            textBounds.x,
            textBounds.y + std::max(0.0F, textBounds.height - size.height) *
                0.5F,
        },
        textClip,
        color,
        commands);
}

void ListControl::paintCellControl(
    const ListCell& value,
    Rect cellRectangle,
    Rect paintClip,
    std::vector<PaintCommand>& commands) const {
    if (value.kind == ListCellKind::Text) {
        return;
    }
    const float availableHeight = std::max(
        0.0F, cellRectangle.height - style_.cellPadding.vertical());
    const float size = std::min(
        style_.controlSize,
        std::min(cellRectangle.width, availableHeight));
    Rect control{
        cellRectangle.x + cellRectangle.width - style_.cellPadding.right - size,
        cellRectangle.y + (cellRectangle.height - size) * 0.5F,
        size,
        size,
    };
    if (value.kind == ListCellKind::CheckBox) {
        control.x = cellRectangle.x + style_.cellPadding.left;
    }
    const Rect controlClip = intersect(control, intersect(paintClip, clip()));
    if (!hasArea(controlClip)) {
        return;
    }
    commands.push_back({
        control, controlClip, style_.controlBackground,
        invalidTextureId, 3.0F,
    });

    if (value.kind == ListCellKind::CheckBox && value.checked) {
        const float markInset = size * 0.25F;
        commands.push_back({
            inset(control, markInset),
            controlClip,
            style_.controlAccent,
            invalidTextureId,
            2.0F,
        });
        return;
    }

    const float unit = std::max(1.0F, size * 0.12F);
    if (value.kind == ListCellKind::Lookup) {
        for (int index = 0; index < 3; ++index) {
            commands.push_back({
                {
                    control.x + size * (0.28F + 0.22F * index),
                    control.y + size * 0.5F - unit * 0.5F,
                    unit,
                    unit,
                },
                controlClip, style_.controlAccent, invalidTextureId, unit * 0.5F,
            });
        }
    } else if (value.kind == ListCellKind::ComboBox) {
        commands.push_back({
            {
                control.x + size * 0.28F,
                control.y + size * 0.42F,
                size * 0.44F,
                std::max(1.0F, size * 0.12F),
            },
            controlClip, style_.controlAccent, invalidTextureId, 1.0F,
        });
        commands.push_back({
            {
                control.x + size * 0.39F,
                control.y + size * 0.57F,
                size * 0.22F,
                std::max(1.0F, size * 0.10F),
            },
            controlClip, style_.controlAccent, invalidTextureId, 1.0F,
        });
    } else if (value.kind == ListCellKind::ActionButton) {
        for (int index = 0; index < 3; ++index) {
            commands.push_back({
                {
                    control.x + size * 0.47F,
                    control.y + size * (0.25F + 0.24F * index),
                    unit,
                    unit,
                },
                controlClip, style_.controlAccent, invalidTextureId, unit * 0.5F,
            });
        }
    }
}

} // namespace lotui
