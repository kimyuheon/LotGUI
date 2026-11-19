#pragma once

#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"
#include "widgets/text_field.h"
#include "text/text_layout.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace lotui {

enum class PropertyAccess { ReadWrite, ReadOnly, WriteOnly };

struct PropertyPoint { double x{0.0}, y{0.0}, z{0.0}; };

struct PropertyFieldOptions {
    std::string label;
    PropertyAccess access{PropertyAccess::ReadWrite};
    bool pointPicker{false};
    bool calculator{false};
};

struct PropertyFieldStyle {
    PropertyFieldStyle();
    TextFieldStyle input{};
    TextFieldStyle readOnlyInput{};
    ButtonStyle action{};
    Color label{0.80F, 0.84F, 0.88F, 1.0F};
    Color readOnlyLabel{0.59F, 0.65F, 0.71F, 1.0F};
    Color actionText{0.86F, 0.92F, 0.96F, 1.0F};
};

// An editor row independent of the host's scene and property storage.
// Picking is asynchronous: the host receives a request, then calls acceptPoint.
class PropertyField final : public LinearLayout {
public:
    using ValueHandler = std::function<void(std::string_view)>;
    using PickHandler = std::function<void()>;
    using ErrorHandler = std::function<void(std::string_view)>;

    PropertyField(std::shared_ptr<const TextEngine> textEngine,
        PropertyFieldOptions options, std::string value = {},
        ValueHandler onValue = {}, PickHandler onPick = {},
        ErrorHandler onError = {}, TextStyle textStyle = {});

    PropertyAccess access() const noexcept;
    void setAccess(PropertyAccess access);
    void setValue(std::string value);
    // No readable value is exposed for write-only fields.
    std::optional<std::string> value() const;
    bool submit();
    bool acceptPoint(PropertyPoint point);
    void requestPick();
    void setStyle(PropertyFieldStyle style);
    const PropertyFieldStyle& style() const noexcept;
    TextField& input() noexcept;
    const TextField& input() const noexcept;

private:
    void reportError(std::string_view message);
    PropertyFieldOptions options_;
    PropertyFieldStyle style_{};
    Label* label_{nullptr};
    TextField* input_{nullptr};
    Button* pickButton_{nullptr};
    Button* calcButton_{nullptr};
    Label* pickLabel_{nullptr};
    Label* calcLabel_{nullptr};
    ValueHandler onValue_;
    PickHandler onPick_;
    ErrorHandler onError_;
};

// Basic arithmetic for numeric properties: +, -, *, / and parentheses.
std::optional<double> evaluatePropertyExpression(std::string_view expression);

} // namespace lotui
