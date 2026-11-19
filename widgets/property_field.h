#pragma once

#include "widgets/linear_layout.h"
#include "text/text_layout.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace lotui {

class TextField;
class Button;

enum class PropertyAccess { ReadWrite, ReadOnly, WriteOnly };

struct PropertyPoint { double x{0.0}, y{0.0}, z{0.0}; };

struct PropertyFieldOptions {
    std::string label;
    PropertyAccess access{PropertyAccess::ReadWrite};
    bool pointPicker{false};
    bool calculator{false};
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
    TextField& input() noexcept;
    const TextField& input() const noexcept;

private:
    void reportError(std::string_view message);
    PropertyFieldOptions options_;
    TextField* input_{nullptr};
    Button* pickButton_{nullptr};
    Button* calcButton_{nullptr};
    ValueHandler onValue_;
    PickHandler onPick_;
    ErrorHandler onError_;
};

// Basic arithmetic for numeric properties: +, -, *, / and parentheses.
std::optional<double> evaluatePropertyExpression(std::string_view expression);

} // namespace lotui
