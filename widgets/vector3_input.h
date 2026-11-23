#pragma once

#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/numeric_input.h"

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace lotui {

struct Vector3InputLayout {
    float labelWidth{18.0F};
    float trailingWidth{0.0F};
    float spacing{7.0F};
    float rowSpacing{4.0F};
    float minimumFieldWidth{0.0F};
};

// Three aligned scalar editors for CAD positions, rotations, and scale.
// The host owns model changes and Undo transactions through onAxisChanged.
class Vector3Input final : public LinearLayout {
public:
    using ChangedHandler = std::function<void(std::size_t, double)>;

    Vector3Input(std::shared_ptr<const TextEngine> textEngine,
        NumericInputOptions options = {}, ChangedHandler onAxisChanged = {},
        NumericInputStyle inputStyle = {}, TextStyle textStyle = {},
        Vector3InputLayout layout = {})
        : LinearLayout(false) {
        LinearLayoutOptions columnOptions;
        columnOptions.spacing = layout.rowSpacing;
        columnOptions.crossAxisAlignment = CrossAxisAlignment::Stretch;
        setOptions(columnOptions);
        const float height = options.preferredSize.height;
        for (std::size_t axis = 0; axis < fields_.size(); ++axis) {
            auto row = std::make_unique<Row>();
            LinearLayoutOptions rowOptions;
            rowOptions.spacing = layout.spacing;
            rowOptions.crossAxisAlignment = CrossAxisAlignment::Stretch;
            row->setOptions(rowOptions);
            row->addChild(std::make_unique<Label>(textEngine,
                std::string(1, "XYZ"[axis]), textStyle),
                {0.0F, {layout.labelWidth, height},
                    {layout.labelWidth, height}});
            auto field = std::make_unique<NumericInput>(textEngine, options,
                [onAxisChanged, axis](double value) {
                    if (onAxisChanged) onAxisChanged(axis, value);
                }, inputStyle, textStyle);
            fields_[axis] = field.get();
            row->addChild(std::move(field), {1.0F,
                {layout.minimumFieldWidth, height},
                {unboundedLayoutSize, height}});
            if (layout.trailingWidth > 0.0F)
                row->addChild(std::make_unique<Label>(
                    textEngine, "", textStyle),
                    {0.0F, {layout.trailingWidth, height},
                        {layout.trailingWidth, height}});
            addChild(std::move(row), {0.0F,
                {0.0F, height}, {unboundedLayoutSize, height}});
        }
    }

    NumericInput& axis(std::size_t index) { return *fields_.at(index); }
    const NumericInput& axis(std::size_t index) const {
        return *fields_.at(index);
    }

    void setValue(const std::array<double, 3>& value) {
        for (std::size_t index = 0; index < fields_.size(); ++index)
            fields_[index]->setValue(value[index]);
    }

    std::array<double, 3> value() const noexcept {
        return {fields_[0]->value(), fields_[1]->value(), fields_[2]->value()};
    }

    void setEnabled(bool enabled) noexcept {
        for (auto* field : fields_) field->setEnabled(enabled);
    }

private:
    std::array<NumericInput*, 3> fields_{};
};

} // namespace lotui
